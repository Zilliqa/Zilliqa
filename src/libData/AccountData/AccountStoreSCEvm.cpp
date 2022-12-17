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
#include "AccountStoreSC.h"
#include "EvmClient.h"
#include "common/Constants.h"
#include "libCrypto/EthCrypto.h"
#include "libEth/utils/EthUtils.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnExtras.h"
#include "libUtils/Tracing.h"

namespace {

zil::metrics::uint64Counter_t& GetInvocationsCounter() {
  static auto counter = Metrics::GetInstance().CreateInt64Metric(
      "zilliqa_accountstore", "invocations_count", "Metrics for AccountStore",
      "Blocks");
  return counter;
}

}  // namespace


void AccountStoreSC::EvmCallRunner(const INVOKE_TYPE /*invoke_type*/,  //
                                        const evm::EvmArgs& args,           //
                                        bool& ret,                          //
                                        TransactionReceipt& receipt,        //
                                        evm::EvmResult& result) {
  INCREMENT_METHOD_CALLS_COUNTER(GetInvocationsCounter(), ACCOUNTSTORE_EVM)
  auto scoped_span = trace_api::Scope(zil::trace::Tracing::GetInstance().get_tracer()->StartSpan("EvmCallRunner"));


  //
  // create a worker to be executed in the async method
  const auto worker = [&args, &ret, &result]() -> void {
    try {
      ret = EvmClient::GetInstance().CallRunner(EvmUtils::GetEvmCallJson(args),
                                                result);
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING, "Exception from underlying RPC call " << e.what());
    } catch (...) {
      LOG_GENERAL(WARNING, "UnHandled Exception from underlying RPC call ");
    }
  };

  const auto fut = std::async(std::launch::async, worker);
  // check the future return and when time out log error.
  switch (fut.wait_for(std::chrono::seconds(EVM_RPC_TIMEOUT_SECONDS))) {
    case std::future_status::ready: {
      LOG_GENERAL(WARNING, "lock released normally");

      INCREMENT_CALLS_COUNTER(GetInvocationsCounter(), ACCOUNTSTORE_EVM, "lock",
                              "release-normal");
    } break;
    case std::future_status::timeout: {
      LOG_GENERAL(WARNING, "Txn processing timeout!");

      if (LAUNCH_EVM_DAEMON) {
        EvmClient::GetInstance().Reset();
      }

      INCREMENT_CALLS_COUNTER(GetInvocationsCounter(), ACCOUNTSTORE_EVM, "lock",
                              "release-timeout");

      receipt.AddError(EXECUTE_CMD_TIMEOUT);
      ret = false;
    } break;
    case std::future_status::deferred: {
      LOG_GENERAL(WARNING, "Illegal future return status!");

      INCREMENT_CALLS_COUNTER(GetInvocationsCounter(), ACCOUNTSTORE_EVM, "lock",
                              "release-deferred");

      ret = false;
    }
  }
}


uint64_t AccountStoreSC::InvokeEvmInterpreter(
    Account* contractAccount, INVOKE_TYPE invoke_type, const evm::EvmArgs& args,
    bool& ret, TransactionReceipt& receipt, evm::EvmResult& result) {
  // call evm-ds
  EvmCallRunner(invoke_type, args, ret, receipt, result);

  if (result.exit_reason().exit_reason_case() !=
      evm::ExitReason::ExitReasonCase::kSucceed) {
    LOG_GENERAL(WARNING, EvmUtils::ExitReasonString(result.exit_reason()));
    ret = false;
  }

  if (result.logs_size() > 0) {
    Json::Value entry = Json::arrayValue;

    for (const auto& log : result.logs()) {
      Json::Value logJson;
      logJson["address"] = "0x" + ProtoToAddress(log.address()).hex();
      logJson["data"] = "0x" + boost::algorithm::hex(log.data());
      Json::Value topics_array = Json::arrayValue;
      for (const auto& topic : log.topics()) {
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
  for (const auto& it : result.apply()) {
    Address address;
    Account* targetAccount;
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
          for (const auto& x : states) {
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
        const std::string& code = it.modify().code();
        if (!code.empty()) {
          targetAccount->SetImmutable(
              DataConversion::StringToCharArray("EVM" + code), {});
        }

        // Actually Update the state for the contract
        for (const auto& sit : it.modify().storage()) {
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


bool AccountStoreSC::ViewAccounts(const evm::EvmArgs& args,
                                       evm::EvmResult& result) {
  return EvmClient::GetInstance().CallRunner(EvmUtils::GetEvmCallJson(args),
                                             result);
}

static std::string txnIdToString(const TxnHash& txn) {
  std::ostringstream str;
  str << "0x" << txn;
  return str.str();
}


bool AccountStoreSC::UpdateAccountsEvm(
    const uint64_t& blockNum, const unsigned int& numShards, const bool& isDS,
    const Transaction& transaction, const TxnExtras& txnExtras,
    TransactionReceipt& receipt, TxnStatus& error_code) {
  LOG_MARKER();

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Process txn: " << transaction.GetTranID());
  }

  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_curIsDS = isDS;
  m_txnProcessTimeout = false;
  error_code = TxnStatus::NOT_PRESENT;
  const Address fromAddr = transaction.GetSenderAddr();

  uint64_t gasLimitEth = transaction.GetGasLimitEth();

  // Get the amount of deposit for running this txn
  uint256_t gasDepositWei;
  if (!SafeMath<uint256_t>::mul(transaction.GetGasLimitZil(),
                                transaction.GetGasPriceWei(), gasDepositWei)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::CONTRACT_CREATION: {
      INCREMENT_CALLS_COUNTER(GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "Transaction", "Create");

      if (LOG_SC) {
        LOG_GENERAL(WARNING, "Create contract");
      }

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }
      const auto baseFee = Eth::getGasUnitsForContractDeployment(
          transaction.GetCode(), transaction.GetData());

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (transaction.GetGasLimitEth() < baseFee) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimitEth()
                                          << " less than " << baseFee);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      const uint256_t fromAccountBalance =
          uint256_t{fromAccount->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
      if (fromAccountBalance < gasDepositWei + transaction.GetAmountWei()) {
        LOG_GENERAL(WARNING,
                    "The account doesn't have enough gas to create a contract");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }

      // generate address for new contract account
      Address contractAddress =
          Account::GetAddressForContract(fromAddr, fromAccount->GetNonce(),
                                         transaction.GetVersionIdentifier());
      LOG_GENERAL(INFO, "Contract creation address is " << contractAddress);
      // instantiate the object for contract account
      // ** Remember to call RemoveAccount if deployment failed halfway
      Account* contractAccount;
      // For the EVM, we create the temporary account in Atomics, so that we
      // can remove it easily if something doesn't work out.
      DiscardAtomics();  // We start by making sure our new state is clean.
      if (!this->AddAccountAtomic(contractAddress, {0, 0})) {
        LOG_GENERAL(WARNING, "AddAccount failed for contract address "
                                 << contractAddress.hex());
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }
      contractAccount = this->GetAccountAtomic(contractAddress);
      if (contractAccount == nullptr) {
        LOG_GENERAL(WARNING, "contractAccount is null ptr");
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }
      if (transaction.GetCode().empty()) {
        LOG_GENERAL(WARNING,
                    "Creating a contract with empty code is not feasible.");
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }

      try {
        // TODO verify this line is needed, suspect it is a scilla thing
        m_curBlockNum = blockNum;
        const uint128_t decreaseAmount =
            uint128_t{gasDepositWei / EVM_ZIL_SCALING_FACTOR};
        if (!this->DecreaseBalance(fromAddr, decreaseAmount)) {
          LOG_GENERAL(WARNING, "Evm Decrease Balance has failed");
          error_code = TxnStatus::FAIL_CONTRACT_INIT;
          return false;
        }
      } catch (const std::exception& e) {
        LOG_GENERAL(WARNING,
                    "Evm Exception caught in Decrease Balance " << e.what());
        error_code = TxnStatus::FAIL_CONTRACT_INIT;
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
                            << transaction.GetAmountQa() << " limit "
                            << transaction.GetGasLimitEth());

      if (!TransferBalanceAtomic(fromAddr, contractAddress,
                                 transaction.GetAmountQa())) {
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
        return false;
      }

      evm::EvmArgs args;
      *args.mutable_address() = AddressToProto(contractAddress);
      *args.mutable_origin() = AddressToProto(fromAddr);
      *args.mutable_code() =
          DataConversion::CharArrayToString(StripEVM(transaction.GetCode()));
      *args.mutable_data() =
          DataConversion::CharArrayToString(transaction.GetData());
      // Give EVM only gas provided for code execution excluding constant fees
      args.set_gas_limit(transaction.GetGasLimitEth() - baseFee);
      *args.mutable_apparent_value() =
          UIntToProto(transaction.GetAmountWei().convert_to<uint256_t>());
      *args.mutable_context() = txnIdToString(transaction.GetTranID());
      if (!GetEvmEvalExtras(blockNum, txnExtras, *args.mutable_extras())) {
        LOG_GENERAL(WARNING, "Failed to get EVM call extras");
        error_code = TxnStatus::ERROR;
        return false;
      }

      std::map<std::string, zbytes> t_newmetadata;

      t_newmetadata.emplace(Contract::ContractStorage::GenerateStorageKey(
                                contractAddress, CONTRACT_ADDR_INDICATOR, {}),
                            contractAddress.asBytes());

      if (!contractAccount->UpdateStates(contractAddress, t_newmetadata, {},
                                         true)) {
        LOG_GENERAL(WARNING, "Account::UpdateStates failed");
        return false;
      }

      evm::EvmResult result;

      auto gasRemained =
          InvokeEvmInterpreter(contractAccount, RUNNER_CREATE, args,
                               evm_call_run_succeeded, receipt, result);

      const auto gasRemainedCore = GasConv::GasUnitsFromEthToCore(gasRemained);

      // *************************************************************************
      // Summary
      uint128_t gasRefund;
      if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                    transaction.GetGasPriceWei(), gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
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
        receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemainedCore);
        receipt.update();
        // TODO : confirm we increase nonce on failure
        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
        }

        LOG_GENERAL(INFO,
                    "Executing contract Creation transaction finished "
                    "unsuccessfully");
        return true;
      }

      if (transaction.GetGasLimitZil() < gasRemainedCore) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimitZil()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      /// calculate total gas in receipt
      receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemainedCore);

      break;
    }

    case Transaction::NON_CONTRACT:
    case Transaction::CONTRACT_CALL: {
      INCREMENT_CALLS_COUNTER(GetInvocationsCounter(), ACCOUNTSTORE_EVM,
                              "Transaction", "Contract-Call/Non Contract");

      if (LOG_SC) {
        LOG_GENERAL(WARNING, "Tx is contract call");
      }

      // reset the storageroot update buffer atomic per transaction
      m_storageRootUpdateBufferAtomic.clear();

      m_originAddr = fromAddr;

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      Account* contractAccount = this->GetAccount(transaction.GetToAddr());
      if (contractAccount == nullptr) {
        LOG_GENERAL(WARNING, "The target contract account doesn't exist");
        error_code = TxnStatus::INVALID_TO_ACCOUNT;
        return false;
      }

      // Check if gaslimit meets the minimum requirement for contract call (at
      // least const fee)
      if (transaction.GetGasLimitEth() < MIN_ETH_GAS) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimitEth()
                                          << " less than " << MIN_ETH_GAS);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      LOG_GENERAL(INFO, "Call contract");

      const uint256_t fromAccountBalance =
          uint256_t{fromAccount->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
      if (fromAccountBalance < gasDepositWei + transaction.GetAmountWei()) {
        LOG_GENERAL(WARNING, "The account (balance: "
                                 << fromAccountBalance
                                 << ") "
                                    "has not enough balance to deposit the gas "
                                    "price to deposit ("
                                 << gasDepositWei
                                 << ") "
                                    "and transfer the amount ("
                                 << transaction.GetAmountWei()
                                 << ") in the txn, "
                                    "rejected");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }

      m_curSenderAddr = fromAddr;
      m_curEdges = 0;

      if (contractAccount->GetCode().empty()) {
        LOG_GENERAL(
            WARNING,
            "Trying to call a smart contract that has no code will fail");
        error_code = TxnStatus::NOT_PRESENT;
        return false;
      }

      m_curBlockNum = blockNum;

      DiscardAtomics();
      const uint128_t amountToDecrease =
          uint128_t{gasDepositWei / EVM_ZIL_SCALING_FACTOR};
      if (!this->DecreaseBalance(fromAddr, amountToDecrease)) {
        LOG_GENERAL(WARNING, "DecreaseBalance failed");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      m_curGasLimit = transaction.GetGasLimitZil();
      m_curGasPrice = transaction.GetGasPriceWei();
      m_curContractAddr = transaction.GetToAddr();
      m_curAmount = transaction.GetAmountQa();
      m_curNumShards = numShards;

      std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      Contract::ContractStorage::GetContractStorage().BufferCurrentState();

      std::string runnerPrint;
      bool evm_call_succeeded{true};

      if (!TransferBalanceAtomic(fromAddr, m_curContractAddr,
                                 transaction.GetAmountQa())) {
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
        return false;
      }

      evm::EvmArgs args;
      *args.mutable_address() = AddressToProto(m_curContractAddr);
      *args.mutable_origin() = AddressToProto(fromAddr);
      *args.mutable_code() = DataConversion::CharArrayToString(
          StripEVM(contractAccount->GetCode()));
      *args.mutable_data() =
          DataConversion::CharArrayToString(transaction.GetData());
      // Give EVM only gas provided for code execution excluding constant fee
      args.set_gas_limit(transaction.GetGasLimitEth() - MIN_ETH_GAS);
      *args.mutable_apparent_value() =
          UIntToProto(transaction.GetAmountWei().convert_to<uint256_t>());
      *args.mutable_context() = txnIdToString(transaction.GetTranID());

      if (!GetEvmEvalExtras(blockNum, txnExtras, *args.mutable_extras())) {
        LOG_GENERAL(WARNING, "Failed to get EVM call extras");
        error_code = TxnStatus::ERROR;
        return false;
      }

      LOG_GENERAL(WARNING, "contract address is " << m_curContractAddr
                                                  << " caller account is "
                                                  << fromAddr);
      evm::EvmResult result;
      const uint64_t gasRemained =
          InvokeEvmInterpreter(contractAccount, RUNNER_CALL, args,
                               evm_call_succeeded, receipt, result);

      uint64_t gasRemainedCore = GasConv::GasUnitsFromEthToCore(gasRemained);

      if (!evm_call_succeeded) {
        Contract::ContractStorage::GetContractStorage().RevertPrevState();
        DiscardAtomics();
        gasRemainedCore =
            std::min(transaction.GetGasLimitZil(), gasRemainedCore);
      } else {
        CommitAtomics();
      }
      uint128_t gasRefund;
      if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                    transaction.GetGasPriceWei(), gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      if (!this->IncreaseBalance(fromAddr,

                                 gasRefund / EVM_ZIL_SCALING_FACTOR)) {
        LOG_GENERAL(WARNING, "IncreaseBalance failed for gasRefund");
      }

      if (transaction.GetGasLimitZil() < gasRemainedCore) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimitZil()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      // TODO: the cum gas might not be applied correctly (should be block
      // level)
      receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemainedCore);
      if (!evm_call_succeeded) {
        receipt.SetResult(false);
        receipt.CleanEntry();
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
          LOG_GENERAL(WARNING, "Increase Nonce failed on bad txn");
          return false;
        }
        return true;
      }
    } break;

    default: {
      LOG_GENERAL(WARNING, "Txn is not typed correctly")
      error_code = TxnStatus::INCORRECT_TXN_TYPE;
      return false;
    }
    case Transaction::ERROR:
      // TODO
      // maybe we should treat this error properly we have just fallen through.
      LOG_GENERAL(WARNING,
                  "Txn does not appear to be valid! Nothing has been executed.")
      break;
  }

  if (!this->IncreaseNonce(fromAddr)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  receipt.SetResult(true);
  receipt.update();

  // since txn succeeded, commit the atomic buffer. If no updates, it is a noop.
  m_storageRootUpdateBuffer.insert(m_storageRootUpdateBufferAtomic.begin(),
                                   m_storageRootUpdateBufferAtomic.end());

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Executing contract transaction finished");
    LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
  }

  return true;
}


bool AccountStoreSC::AddAccountAtomic(const Address& address,
                                           const Account& account) {
  return m_accountStoreAtomic->AddAccount(address, account);
}
