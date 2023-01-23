/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
//
#include <json/value.h>
#include <chrono>
#include <future>
#include <stdexcept>
#include <vector>

#include "opentelemetry/context/propagation/text_map_propagator.h"

#include "AccountStoreSC.h"
#include "common/Constants.h"
#include "common/TraceFilters.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountStore/services/evm/EvmClient.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libEth/utils/EthUtils.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/Tracing.h"
#include "libUtils/TxnExtras.h"

namespace metric_sdk = opentelemetry::sdk::metrics;

namespace evm {

zil::metrics::uint64Counter_t &GetInvocationsCounter() {
  static auto counter = Metrics::GetInstance().CreateInt64Metric(
      "zilliqa.evm.invoke", "invocations_count", "Metrics for AccountStore",
      "calls");
  return counter;
}

// Define it as a const because we need to set a view for the boundaries with
// the same name
const char *EVM_HISTOGRAM = "zilliqa.evm.histogram";

zil::metrics::doubleHistogram_t &GetHistogramCounter() {
  static auto histogram = Metrics::GetMeter()->CreateDoubleHistogram(
      EVM_HISTOGRAM, "evm latency histogram", "ms");
  return histogram;
}

}  // namespace evm

void AccountStoreSC::EvmCallRunner(const INVOKE_TYPE /*invoke_type*/,  //
                                   const evm::EvmArgs &args,           //
                                   bool &ret,                          //
                                   TransactionReceipt &receipt,        //
                                   evm::EvmResult &result) {
  INCREMENT_METHOD_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM)

  auto span = START_SPAN(ACC_EVM, {});
  SCOPED_SPAN(ACC_EVM, scope, span);

  namespace context = opentelemetry::context;

  //
  // create a worker to be executed in the async method
  const auto worker = [&span, &args, &ret, &result]() -> void {
    try {
      ret = EvmClient::GetInstance().CallRunner(EvmUtils::GetEvmCallJson(args),
                                                result);
    } catch (std::exception &e) {
      std::stringstream ss;
      ss << "Exception from underlying RPC call " << e.what();
      Metrics::GetInstance().CaptureEMT(
          span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
          zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
          ss.str());

    } catch (...) {
      std::stringstream ss;
      ss << "UnHandled Exception from underlying RPC call ";
      Metrics::GetInstance().CaptureEMT(
          span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
          zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
          ss.str());
    }
  };

  const auto fut = std::async(std::launch::async, worker);
  // check the future return and when time out log error.
  switch (fut.wait_for(std::chrono::seconds(EVM_RPC_TIMEOUT_SECONDS))) {
    case std::future_status::ready: {
      LOG_GENERAL(WARNING, "lock released normally");

      INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "lock", "release-normal");

    } break;
    case std::future_status::timeout: {
      LOG_GENERAL(WARNING, "Txn processing timeout!");

      if (LAUNCH_EVM_DAEMON) {
        EvmClient::GetInstance().Reset();
      }

      INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "lock", "release-timeout");

      auto constexpr str = "Timeout on lock waiting for EVM-DS";
      LOG_GENERAL(WARNING, str);
      TRACE_ATTRIBUTE msg{{"reason", str}};
      TRACE_EVENT(span, ACC_EVM, "return", msg);

      receipt.AddError(EXECUTE_CMD_TIMEOUT);
      ret = false;
    } break;
    case std::future_status::deferred: {
      LOG_GENERAL(WARNING, "Illegal future return status!");

      INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "lock", "release-deferred");

      auto constexpr str = "Illegal future return status";
      LOG_GENERAL(WARNING, str);
      TRACE_ATTRIBUTE msg{{"reason", str}};
      TRACE_EVENT(span, ACC_EVM, "return", msg);

      ret = false;
    }
  }
}

uint64_t AccountStoreSC::InvokeEvmInterpreter(
    Account *contractAccount, INVOKE_TYPE invoke_type, const evm::EvmArgs &args,
    bool &ret, TransactionReceipt &receipt, evm::EvmResult &result) {
  // call evm-ds
  EvmCallRunner(invoke_type, args, ret, receipt, result);

  if (result.exit_reason().exit_reason_case() !=
      evm::ExitReason::ExitReasonCase::kSucceed) {
    LOG_GENERAL(WARNING, EvmUtils::ExitReasonString(result.exit_reason()));
    ret = false;
  }

  if (result.logs_size() > 0) {
    Json::Value entry = Json::arrayValue;

    for (const auto &log : result.logs()) {
      Json::Value logJson;
      logJson["address"] = "0x" + ProtoToAddress(log.address()).hex();
      logJson["data"] = "0x" + boost::algorithm::hex(log.data());
      Json::Value topics_array = Json::arrayValue;
      for (const auto &topic : log.topics()) {
        topics_array.append("0x" + ProtoToH256(topic).hex());
      }
      logJson["topics"] = topics_array;
      entry.append(logJson);
    }
    receipt.AddJsonEntry(entry);
  }

  std::map<std::string, zbytes> states;
  std::vector<std::string> toDeletes;
  // parse the return values from the call to evm.
  for (const auto &it : result.apply()) {
    Address address;
    Account *targetAccount;
    switch (it.apply_case()) {
      case evm::Apply::ApplyCase::kDelete:
        // Set account balance to 0 to avoid any leakage of funds in case
        // selfdestruct is called multiple times
        address = ProtoToAddress(it.delete_().address());
        targetAccount = this->GetAccountAtomic(address);
        targetAccount->SetBalance(uint128_t(0));
        m_storageRootUpdateBufferAtomic.emplace(address);
        break;
      case evm::Apply::ApplyCase::kModify: {
        // Get the account that this apply instruction applies to
        address = ProtoToAddress(it.modify().address());
        targetAccount = this->GetAccountAtomic(address);
        if (targetAccount == nullptr) {
          if (!this->AddAccountAtomic(address, {0, 0})) {
            LOG_GENERAL(WARNING,
                        "AddAccount failed for address " << address.hex());
            continue;
          }
          targetAccount = this->GetAccountAtomic(address);
          if (targetAccount == nullptr) {
            LOG_GENERAL(WARNING, "failed to retrieve new account for address "
                                     << address.hex());
            continue;
          }
        }

        if (it.modify().reset_storage()) {
          states.clear();
          toDeletes.clear();

          Contract::ContractStorage::GetContractStorage()
              .FetchStateDataForContract(states, address, "", {}, true);
          for (const auto &x : states) {
            toDeletes.emplace_back(x.first);
          }

          if (!targetAccount->UpdateStates(address, {}, toDeletes, true)) {
            LOG_GENERAL(
                WARNING,
                "Failed to update states hby setting indices for deletion "
                "for "
                    << address);
          }
        }

        // If Instructed to reset the Code do so and call SetImmutable to reset
        // the hash
        const std::string &code = it.modify().code();
        if (!code.empty()) {
          targetAccount->SetImmutable(
              DataConversion::StringToCharArray("EVM" + code), {});
        }

        // Actually Update the state for the contract
        for (const auto &sit : it.modify().storage()) {
          LOG_GENERAL(INFO, "Saving storage for Address: " << address);
          if (not Contract::ContractStorage::GetContractStorage()
                      .UpdateStateValue(
                          address, DataConversion::StringToCharArray(sit.key()),
                          0, DataConversion::StringToCharArray(sit.value()),
                          0)) {
            LOG_GENERAL(WARNING,
                        "Failed to update state value at address " << address);
          }
        }

        if (it.modify().has_balance()) {
          uint256_t balance = ProtoToUint(it.modify().balance());
          if ((balance >> 128) > 0) {
            throw std::runtime_error("Balance overflow!");
          }
          targetAccount->SetBalance(balance.convert_to<uint128_t>());
        }
        if (it.modify().has_nonce()) {
          uint256_t nonce = ProtoToUint(it.modify().nonce());
          if ((nonce >> 64) > 0) {
            throw std::runtime_error("Nonce overflow!");
          }
          targetAccount->SetNonce(nonce.convert_to<uint64_t>());
        }
        // Mark the Address as updated
        m_storageRootUpdateBufferAtomic.emplace(address);
      } break;
      case evm::Apply::ApplyCase::APPLY_NOT_SET:
        // do nothing;
        break;
    }
  }

  // send code to be executed
  if (invoke_type == RUNNER_CREATE) {
    contractAccount->SetImmutable(
        DataConversion::StringToCharArray("EVM" + result.return_value()),
        contractAccount->GetInitData());
  }

  return result.remaining_gas();
}

/*
 * EvmProcessMessage()
 *
 * Direct call into processing without using legacy transaction.
 *
 */

bool AccountStoreSC::EvmProcessMessage(EvmProcessContext &params,
                                       evm::EvmResult &result) {
  unsigned int unused_numShards = 0;
  bool unused_isds = true;
  TransactionReceipt rcpt;
  TxnStatus error_code;
  std::chrono::system_clock::time_point tpStart;

  INCREMENT_METHOD_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM)

  auto span = START_SPAN(ACC_EVM, {});

  tpStart = r_timer_start();

  bool status = UpdateAccountsEvm(params.GetBlockNumber(), unused_numShards,
                                  unused_isds, rcpt, error_code, params);

  if (METRICS_ENABLED(ACCOUNTSTORE_EVM)) {
    auto val = r_timer_end(tpStart);
    if (val > 0) {
      m_storeMetrics.evmCall = val;
    }
  }

  result = params.GetEvmResult();
  params.SetEvmReceipt(rcpt);

  if (TRACE_ENABLED(ACC_EVM)) {
    span->End();
  }

  return status;
}

bool AccountStoreSC::UpdateAccountsEvm(const uint64_t &blockNum,
                                       const unsigned int &numShards,
                                       const bool &isDS,
                                       TransactionReceipt &receipt,
                                       TxnStatus &error_code,
                                       EvmProcessContext &evmContext) {
  LOG_MARKER();
  CALLS_LATENCY_MARKER(evm::GetInvocationsCounter(), evm::GetHistogramCounter(),
                       zil::metrics::FilterClass::ACCOUNTSTORE_EVM);

  // store into the metric holder.
  if (blockNum > 0) m_storeMetrics.blockNumber = blockNum;

  LOG_GENERAL(INFO,
              "Commit Context Mode="
                  << (evmContext.GetCommit() ? "Commit" : "Non-Commital"));

  std::string txnId = evmContext.GetTranID().hex();

  TRACE_ATTRIBUTE am{{"tid", txnId}, {"block", blockNum}};
  auto span = START_SPAN(ACC_EVM, am);
  SCOPED_SPAN(ACC_EVM, scope, span);

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Process txn: " << evmContext.GetTranID());
  }

  if (evmContext.GetDirect()) {
    evm::EvmResult res;
    bool status = EvmClient::GetInstance().CallRunner(
        EvmUtils::GetEvmCallJson(evmContext.GetEvmArgs()), res);
    evmContext.SetEvmResult(res);
    return status;
  }

  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_curIsDS = isDS;
  m_txnProcessTimeout = false;
  error_code = TxnStatus::NOT_PRESENT;
  const Address fromAddr = evmContext.GetTransaction().GetSenderAddr();
  uint64_t gasLimitEth = evmContext.GetTransaction().GetGasLimitEth();

  // Get the amount of deposit for running this txn
  uint256_t gasDepositWei;
  if (!SafeMath<uint256_t>::mul(evmContext.GetTransaction().GetGasLimitZil(),
                                evmContext.GetTransaction().GetGasPriceWei(),
                                gasDepositWei)) {
    error_code = TxnStatus::MATH_ERROR;
    auto constexpr str = "Math Error";
    LOG_GENERAL(WARNING, str);
    TRACE_ATTRIBUTE msg{{"Gas", evmContext.GetTransaction().GetGasLimitEth()},
                        {"reason", str}};
    TRACE_EVENT(span, ACC_EVM, "return", msg);
    return false;
  }

  m_curIsDS = isDS;
  m_txnProcessTimeout = false;

  switch (evmContext.GetContractType()) {
    case Transaction::CONTRACT_CREATION: {
      INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "Transaction", "Create");

      if (LOG_SC) {
        LOG_GENERAL(WARNING, "Create contract");
      }

      Account *fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            "Sender has no balance, reject");

        return false;
      }
      const auto baseFee = Eth::getGasUnitsForContractDeployment(
          evmContext.GetCode(), evmContext.GetData());

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (evmContext.GetTransaction().GetGasLimitEth() < baseFee) {
        LOG_GENERAL(WARNING, "Gas limit "
                                 << evmContext.GetTransaction().GetGasLimitEth()
                                 << " less than " << baseFee);

        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            "Insufficient Gas");

        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      const uint256_t fromAccountBalance =
          uint256_t{fromAccount->GetBalance()} * EVM_ZIL_SCALING_FACTOR;

      if (fromAccountBalance <
          gasDepositWei + evmContext.GetTransaction().GetAmountWei()) {
        std::stringstream ss;
        ss << "The account doesn't have enough gas to create a contract : "
           << gasDepositWei;
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            ss.str());
        return false;
      }

      // generate address for new contract account
      Address contractAddress = Account::GetAddressForContract(
          fromAddr, fromAccount->GetNonce(),
          evmContext.GetTransaction().GetVersionIdentifier());

      LOG_GENERAL(INFO, "Contract creation address is " << contractAddress);
      // instantiate the object for contract account
      // ** Remember to call RemoveAccount if deployment failed halfway
      Account *contractAccount;
      // For the EVM, we create the temporary account in Atomics, so that we
      // can remove it easily if something doesn't work out.
      DiscardAtomics();  // We start by making sure our new state is clean.
      if (!this->AddAccountAtomic(contractAddress, {0, 0})) {
        constexpr auto str = "AddAccount failed for contract address ";
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);
        return false;
      }
      contractAccount = this->GetAccountAtomic(contractAddress);
      if (contractAccount == nullptr) {
        constexpr auto str = "contractAccount is null ptr";
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            str);
        return false;
      }
      if (evmContext.GetCode().empty()) {
        constexpr auto str =
            "Creating a contract with empty code is not feasible.";
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            str);
        return false;
      }

      try {
        m_curBlockNum = blockNum;
        const uint128_t decreaseAmount =
            uint128_t{gasDepositWei / EVM_ZIL_SCALING_FACTOR};
        if (!this->DecreaseBalance(fromAddr, decreaseAmount)) {
          error_code = TxnStatus::FAIL_CONTRACT_INIT;
          constexpr auto str = "Decrease Balance failed.";
          Metrics::GetInstance().CaptureEMT(
              span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
              zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
              str, error_code);
          return false;
        }
      } catch (const std::exception &e) {
        std::stringstream ss;
        ss << "Evm Exception caught in Decrease Balance " << e.what();
        error_code = TxnStatus::FAIL_CONTRACT_INIT;
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            ss.str(), error_code);

        return false;
      }

      std::map<std::string, zbytes> t_metadata;
      t_metadata.emplace(
          Contract::ContractStorage::GetContractStorage().GenerateStorageKey(
              contractAddress, SCILLA_VERSION_INDICATOR, {}),
          DataConversion::StringToCharArray("0"));

      // *************************************************************************
      // Undergo a runner
      bool evm_call_run_succeeded{true};

      LOG_GENERAL(INFO, "Invoking EVM with Cumulative Gas "
                            << gasLimitEth << " alleged "
                            << evmContext.GetTransaction().GetAmountQa()
                            << " limit "
                            << evmContext.GetTransaction().GetGasLimitEth());

      if (!TransferBalanceAtomic(fromAddr, contractAddress,
                                 evmContext.GetTransaction().GetAmountQa())) {
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        const std::string str{"TransferBalance Atomic failed"};

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        return false;
      }

      std::map<std::string, zbytes> t_newmetadata;

      t_newmetadata.emplace(Contract::ContractStorage::GenerateStorageKey(
                                contractAddress, CONTRACT_ADDR_INDICATOR, {}),
                            contractAddress.asBytes());

      if (!contractAccount->UpdateStates(contractAddress, t_newmetadata, {},
                                         true)) {
        const std::string str{"Account::UpdateStates failed"};
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        return false;
      }

      evm::EvmResult result;
      evmContext.SetContractAddress(contractAddress);
      // Give EVM only gas provided for code execution excluding constant fees
      evmContext.SetGasLimit(evmContext.GetTransaction().GetGasLimitEth() -
                             baseFee);
      auto gasRemained = InvokeEvmInterpreter(
          contractAccount, RUNNER_CREATE, evmContext.GetEvmArgs(),
          evm_call_run_succeeded, receipt, result);

      evmContext.SetEvmResult(result);
      const auto gasRemainedCore = GasConv::GasUnitsFromEthToCore(gasRemained);

      // *************************************************************************
      // Summary
      uint128_t gasRefund;
      if (!SafeMath<uint128_t>::mul(
              gasRemainedCore, evmContext.GetTransaction().GetGasPriceWei(),
              gasRefund)) {
        const std::string str{"Math Error"};

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        return false;
      }

      if (!this->IncreaseBalance(fromAddr,
                                 gasRefund / EVM_ZIL_SCALING_FACTOR)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for gasRefund");
      }
      if (evm_call_run_succeeded) {
        CommitAtomics();
      } else {
        DiscardAtomics();

        receipt.SetResult(false);
        receipt.AddError(RUNNER_FAILED);
        receipt.SetCumGas(evmContext.GetTransaction().GetGasLimitZil() -
                          gasRemainedCore);
        receipt.update();
        // TODO : confirm we increase nonce on failure
        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
        }

        auto constexpr str =
            "Executing contract Creation transaction finished unsuccessfully";

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);
        return true;
      }

      if (evmContext.GetTransaction().GetGasLimitZil() < gasRemainedCore) {
        std::stringstream ss;
        ss << "Cumulative Gas calculated Underflow, gasLimit: "
           << evmContext.GetTransaction().GetGasLimitZil()
           << " gasRemained: " << gasRemained << ". Must be something wrong!";

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            ss.str(), error_code);
        return false;
      }

      /// calculate total gas in receipt
      receipt.SetCumGas(evmContext.GetTransaction().GetGasLimitZil() -
                        gasRemainedCore);

      break;
    }

    case Transaction::NON_CONTRACT:
    case Transaction::CONTRACT_CALL: {
      INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "Transaction", "Contract-Call/Non Contract");

      if (LOG_SC) {
        LOG_GENERAL(WARNING, "Tx is contract call");
      }

      // reset the storageroot update buffer atomic per transaction
      m_storageRootUpdateBufferAtomic.clear();

      m_originAddr = fromAddr;

      Account *fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        auto constexpr str = "Sender has no balance, reject";

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      Account *contractAccount =
          this->GetAccount(evmContext.GetTransaction().GetToAddr());
      if (contractAccount == nullptr) {
        error_code = TxnStatus::INVALID_TO_ACCOUNT;
        auto constexpr str = "The target contract account doesn't exist";

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        return false;
      }

      // Check if gaslimit meets the minimum requirement for contract call (at
      // least const fee)
      if (evmContext.GetTransaction().GetGasLimitEth() < MIN_ETH_GAS) {
        std::stringstream ss;
        ss << "Gas limit " << evmContext.GetTransaction().GetGasLimitEth()
           << " less than " << MIN_ETH_GAS;
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            ss.str(), error_code);

        return false;
      }

      LOG_GENERAL(INFO, "Call contract");

      const uint256_t fromAccountBalance =
          uint256_t{fromAccount->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
      if (fromAccountBalance <
          gasDepositWei + evmContext.GetTransaction().GetAmountWei()) {
        std::stringstream ss;
        ss << "The account (balance: " << fromAccountBalance
           << ") "
              "has not enough balance to deposit the gas "
              "price to deposit ("
           << gasDepositWei
           << ") "
              "and transfer the amount ("
           << evmContext.GetTransaction().GetAmountWei()
           << ") in the txn, "
              "rejected";

        error_code = TxnStatus::INSUFFICIENT_BALANCE;

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            ss.str(), error_code);

        return false;
      }

      m_curSenderAddr = fromAddr;
      m_curEdges = 0;

      if (contractAccount->GetCode().empty()) {
        error_code = TxnStatus::NOT_PRESENT;
        auto constexpr str =
            "Trying to call a smart contract that has no code will fail";

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        return false;
      }

      m_curBlockNum = blockNum;

      DiscardAtomics();
      const uint128_t amountToDecrease =
          uint128_t{gasDepositWei / EVM_ZIL_SCALING_FACTOR};
      if (!this->DecreaseBalance(fromAddr, amountToDecrease)) {
        auto constexpr str = "DecreaseBalance failed";
        LOG_GENERAL(WARNING, str);
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);

        return false;
      }

      m_curGasLimit = evmContext.GetTransaction().GetGasLimitZil();
      m_curGasPrice = evmContext.GetTransaction().GetGasPriceWei();
      m_curContractAddr = evmContext.GetTransaction().GetToAddr();
      m_curAmount = evmContext.GetTransaction().GetAmountQa();
      m_curNumShards = numShards;

      std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      Contract::ContractStorage::GetContractStorage().BufferCurrentState();

      std::string runnerPrint;
      bool evm_call_succeeded{true};

      if (!TransferBalanceAtomic(fromAddr, m_curContractAddr,
                                 evmContext.GetTransaction().GetAmountQa())) {
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        auto constexpr str = "TransferBalance Atomic failed";
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);
        return false;
      }

      evmContext.SetCode(contractAccount->GetCode());
      // Give EVM only gas provided for code execution excluding constant fee
      evmContext.SetGasLimit(evmContext.GetTransaction().GetGasLimitEth() -
                             MIN_ETH_GAS);

      LOG_GENERAL(WARNING, "contract address is " << m_curContractAddr
                                                  << " caller account is "
                                                  << fromAddr);
      evm::EvmResult result;
      const uint64_t gasRemained = InvokeEvmInterpreter(
          contractAccount, RUNNER_CALL, evmContext.GetEvmArgs(),
          evm_call_succeeded, receipt, result);

      evmContext.SetEvmResult(result);
      uint64_t gasRemainedCore = GasConv::GasUnitsFromEthToCore(gasRemained);

      if (!evm_call_succeeded) {
        Contract::ContractStorage::GetContractStorage().RevertPrevState();
        DiscardAtomics();
        gasRemainedCore = std::min(evmContext.GetTransaction().GetGasLimitZil(),
                                   gasRemainedCore);
      } else {
        CommitAtomics();
      }
      uint128_t gasRefund;
      if (!SafeMath<uint128_t>::mul(
              gasRemainedCore, evmContext.GetTransaction().GetGasPriceWei(),
              gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
        auto constexpr str = "MATH ERROR";
        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
            error_code);
        return false;
      }

      if (!this->IncreaseBalance(fromAddr,

                                 gasRefund / EVM_ZIL_SCALING_FACTOR)) {
        LOG_GENERAL(WARNING, "IncreaseBalance failed for gasRefund");
      }

      if (evmContext.GetTransaction().GetGasLimitZil() < gasRemainedCore) {
        std::stringstream ss;
        ss << "Cumulative Gas calculated Underflow, gasLimit: "
           << evmContext.GetTransaction().GetGasLimitZil()
           << " gasRemained: " << gasRemained << ". Must be something wrong!";

        error_code = TxnStatus::MATH_ERROR;

        Metrics::GetInstance().CaptureEMT(
            span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
            zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
            ss.str(), error_code);
        return false;
      }

      // TODO: the cum gas might not be applied correctly (should be block
      // level)
      receipt.SetCumGas(evmContext.GetTransaction().GetGasLimitZil() -
                        gasRemainedCore);
      if (!evm_call_succeeded) {
        receipt.SetResult(false);
        receipt.CleanEntry();
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
          auto constexpr str = "Increase Nonce failed on bad txn";
          Metrics::GetInstance().CaptureEMT(
              span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
              zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(),
              str, error_code);
          return false;
        }
        return true;
      }
    } break;

    default: {
      error_code = TxnStatus::INCORRECT_TXN_TYPE;
      auto constexpr str = "CRITICAL Txn is not typed correctly";
      Metrics::GetInstance().CaptureEMT(
          span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
          zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
          error_code);
      return false;
    }
    case Transaction::ERROR:
      auto constexpr str = "Transaction of type ERROR";
      Metrics::GetInstance().CaptureEMT(
          span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
          zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
          error_code);
      break;
  }

  if (!this->IncreaseNonce(fromAddr)) {
    error_code = TxnStatus::MATH_ERROR;
    const std::string str{"Increase Nonce Failed"};

    Metrics::GetInstance().CaptureEMT(
        span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,
        zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), str,
        error_code);

    return false;
  }

  receipt.SetResult(true);
  receipt.update();
  /*
   * Only commit the buffer is commit is enabled.
   *
   * since txn succeeded, commit the atomic buffer. If no updates, it is a
   * noop.
   */

  if (evmContext.GetCommit()) {
    LOG_GENERAL(INFO, "Committing data");
    m_storageRootUpdateBuffer.insert(m_storageRootUpdateBufferAtomic.begin(),
                                     m_storageRootUpdateBufferAtomic.end());
  } else {
    m_storageRootUpdateBuffer.clear();
    DiscardAtomics();
    LOG_GENERAL(INFO, "Not Committing data as commit turned off");
  }

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Executing contract transaction finished");
    LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
  }

  span->SetStatus(opentelemetry::trace::StatusCode::kOk,
                  "Executing contract finished OK");

  return true;
}

bool AccountStoreSC::AddAccountAtomic(const Address &address,
                                      const Account &account) {
  return m_accountStoreAtomic->AddAccount(address, account);
}
