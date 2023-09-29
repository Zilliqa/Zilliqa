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
#include <chrono>

#include <unordered_map>
#include <vector>

#include "libData/AccountStore/AccountStoreCpsInterface.h"
#include "libData/AccountStore/services/scilla/ScillaProcessContext.h"
#include "libScilla/ScillaClient.h"

#include "libPersistence/ContractStorage.h"
#include "libScilla/ScillaIPCServer.h"
#include "libScilla/ScillaUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/SafeMath.h"

#include "libUtils/TimeUtils.h"

#include "libCps/CpsExecutor.h"
#include "libData/AccountStore/AccountStoreSC.h"
#include "libMetrics/Api.h"
#include "libPersistence/BlockStorage.h"

namespace {

Z_I64METRIC &GetInvocationsCounter() {
  static Z_I64METRIC counter{Z_FL::ACCOUNTSTORE_SCILLA, "processors",
                             "Metrics for AccountStore", "Blocks"};
  return counter;
}

}  // namespace
// 5mb
const unsigned int MAX_SCILLA_OUTPUT_SIZE_IN_BYTES = 5120;

#ifdef __APPLE__
typedef long long observerType;
#else
typedef long int observerType;
#endif

namespace zil {
namespace local {

Z_I64METRIC &GetInvocationsCounter() {
  static Z_I64METRIC counter{Z_FL::ACCOUNTSTORE_HISTOGRAMS, "scilla",
                             "Metrics for AccountStore", "calls"};
  return counter;
}

Z_I64GAUGE &GetEvmLatencyCounter() {
  static Z_I64GAUGE counter{Z_FL::ACCOUNTSTORE_EVM, "evm.latency.counter",
                            "Simple EVM latency gauge", "calls", true};
  return counter;
}

Z_I64GAUGE &GetScillaLatencyCounter() {
  static Z_I64GAUGE counter{Z_FL::ACCOUNTSTORE_SCILLA, "scilla_latency_counter",
                            "Simple Scilla latency gauge", "us", true};
  return counter;
}

Z_I64GAUGE &GetProcessorBNCounters() {
  static Z_I64GAUGE counter{Z_FL::ACCOUNTSTORE_EVM, "blocknumber",
                            "Block number seen by processor", "count", true};
  return counter;
}

Z_I64GAUGE &GetProcessorDSBNCounters() {
  static Z_I64GAUGE counter{Z_FL::ACCOUNTSTORE_EVM, "dsblocknumber",
                            "Ds Block number seen by processor", "count", true};
  return counter;
}
}  // namespace local
}  // namespace zil

AccountStoreSC::AccountStoreSC() {
  Metrics::GetInstance();
  m_accountStoreAtomic = std::make_unique<AccountStoreAtomic>(*this);
  m_txnProcessTimeout = false;

  zil::local::GetEvmLatencyCounter().SetCallback([this](auto &&result) {
    if (zil::local::GetEvmLatencyCounter().Enabled()) {
      if (m_stats.evmCall > 0) {
        result.Set(m_stats.evmCall, {});
      }
    }
  });

  zil::local::GetScillaLatencyCounter().SetCallback([this](auto &&result) {
    if (zil::local::GetScillaLatencyCounter().Enabled()) {
      if (m_stats.scillaCall > 0) {
        result.Set(m_stats.scillaCall, {});
      }
    }
  });

  zil::local::GetProcessorBNCounters().SetCallback([this](auto &&result) {
    if (zil::local::GetProcessorBNCounters().Enabled()) {
      if (m_stats.blockNumber > 0) {
        result.Set(m_stats.blockNumber, {});
      }
    }
  });

  zil::local::GetProcessorDSBNCounters().SetCallback([this](auto &&result) {
    if (zil::local::GetProcessorBNCounters().Enabled()) {
      if (m_stats.blockNumberDS > 0) {
        result.Set(m_stats.blockNumberDS, {});
      }
    }
  });
}

void AccountStoreSC::Init() {
  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  AccountStoreBase::Init();
  m_curContractAddr.clear();
  m_curSenderAddr.clear();
  m_curAmount = 0;
  m_curGasLimit = 0;
  m_curGasPrice = 0;
  m_txnProcessTimeout = false;
  std::filesystem::remove_all(EXTLIB_FOLDER);
  std::filesystem::create_directories(EXTLIB_FOLDER);
}

void AccountStoreSC::InvokeInterpreter(
    INVOKE_TYPE invoke_type, std::string &interprinterPrint,
    const uint32_t &version, bool is_library, const uint64_t &available_gas,
    const boost::multiprecision::uint128_t &balance, bool &ret,
    TransactionReceipt &receipt) {
  bool call_already_finished = false;
  std::chrono::system_clock::time_point tpStart = r_timer_start();
  auto func2 = [this, &interprinterPrint, &invoke_type, &version, &is_library,
                &available_gas, &balance,
                &call_already_finished]() mutable -> void {
    switch (invoke_type) {
      case CHECKER:
        if (!ScillaClient::GetInstance().CallChecker(
                version,
                ScillaUtils::GetContractCheckerJson(m_root_w_version,
                                                    is_library, available_gas),
                interprinterPrint)) {
        }
        break;
      case RUNNER_CREATE:
        if (!ScillaClient::GetInstance().CallRunner(
                version,
                ScillaUtils::GetCreateContractJson(m_root_w_version, is_library,
                                                   available_gas, balance),
                interprinterPrint)) {
        }
        break;
      case RUNNER_CALL:
        if (!ScillaClient::GetInstance().CallRunner(
                version,
                ScillaUtils::GetCallContractJson(
                    m_root_w_version, available_gas, balance, is_library),
                interprinterPrint)) {
        }
        break;
      case DISAMBIGUATE:
        if (!ScillaClient::GetInstance().CallDisambiguate(
                version, ScillaUtils::GetDisambiguateJson(),
                interprinterPrint)) {
        }
        break;
    }
    call_already_finished = true;
    m_CallContractConditionVariable.notify_all();
  };
  DetachedFunction(1, func2);

  {
    std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
    if (!call_already_finished) {
      m_CallContractConditionVariable.wait(lk);
    } else {
      LOG_GENERAL(INFO, "Call functions already finished!");
    }
  }

  if (m_txnProcessTimeout) {
    LOG_GENERAL(WARNING, "Txn processing timeout!");

    ScillaClient::GetInstance().CheckClient(version, true);
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    ret = false;
  }
  if (METRICS_ENABLED(ACCOUNTSTORE_SCILLA)) {
    auto val = r_timer_end(tpStart);
    if (val > 0) m_stats.scillaCall = val;
  }
}

bool AccountStoreSC::UpdateAccounts(
    const uint64_t &blockNum, const unsigned int &numShards, const bool &isDS,
    const Transaction &transaction, const TxnExtras &extras,
    TransactionReceipt &receipt, TxnStatus &error_code) {
  INC_CALLS(GetInvocationsCounter());

  LOG_MARKER();

  LOG_GENERAL(INFO, "Process txn: " << transaction.GetTranID());

  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);

  m_curIsDS = isDS;
  m_txnProcessTimeout = false;
  error_code = TxnStatus::NOT_PRESENT;

  if (ENABLE_CPS) {
    LOG_GENERAL(WARNING, "Running Scilla in CPS mode");
    m_originAddr = transaction.GetSenderAddr();
    m_curGasLimit = transaction.GetGasLimitZil();
    m_curGasPrice = transaction.GetGasPriceQa();
    m_curContractAddr = transaction.GetToAddr();
    m_curAmount = transaction.GetAmountQa();
    m_curSenderAddr = transaction.GetSenderAddr();
    m_curEdges = 0;
    m_curNumShards = numShards;
    ScillaProcessContext scillaContext = {
        .origin = transaction.GetSenderAddr(),
        .recipient = transaction.GetToAddr(),
        .code = transaction.GetCode(),
        .data = transaction.GetData(),
        .amount = transaction.GetAmountQa(),
        .gasPrice = transaction.GetGasPriceQa(),
        .gasLimit = transaction.GetGasLimitZil(),
        .blockNum = blockNum,
        .dsBlockNum = m_curDSBlockNum,
        .blockTimestamp = extras.block_timestamp,
        .blockDifficulty = extras.block_difficulty,
        .contractType = Transaction::GetTransactionType(transaction),
        .txnHash = transaction.GetTranID()};

    AccountStoreCpsInterface acCpsInterface{*this};
    libCps::CpsExecutor cpsExecutor{acCpsInterface, receipt};
    auto cpsRunResult = cpsExecutor.RunFromScilla(scillaContext);
    // Scilla runtime could fail but such transactions are not considered as
    // failed
    if (!cpsRunResult.isSuccess &&
        cpsRunResult.txnStatus == TxnStatus::NOT_PRESENT) {
      cpsRunResult.isSuccess = true;
    }
    error_code = cpsRunResult.txnStatus;

    return cpsRunResult.isSuccess;
  }

  const PubKey &senderPubKey = transaction.GetSenderPubKey();
  const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  Address toAddr = transaction.GetToAddr();

  const uint128_t &amount = transaction.GetAmountQa();

  // Initiate gasRemained
  uint64_t gasRemained = transaction.GetGasLimitZil();

  // Get the amount of deposit for running this txn
  uint128_t gasDeposit;
  if (!SafeMath<uint128_t>::mul(gasRemained, transaction.GetGasPriceQa(),
                                gasDeposit)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::NON_CONTRACT: {
      // LOG_GENERAL(INFO, "Normal transaction");
      INC_STATUS(GetInvocationsCounter(), "Transaction", "Non-Contract");

      // Disallow normal transaction to contract account
      Account *toAccount = this->GetAccount(toAddr);
      if (toAccount != nullptr) {
        if (toAccount->isContract()) {
          LOG_GENERAL(WARNING, "Contract account won't accept normal txn");
          error_code = TxnStatus::INVALID_TO_ACCOUNT;
          return false;
        }
      }

      return AccountStoreBase::UpdateAccounts(transaction, receipt, error_code);
    }
    case Transaction::CONTRACT_CREATION: {
      LOG_GENERAL(INFO, "Create contract");

      INC_STATUS(GetInvocationsCounter(), "Transaction", "Create");

      // bool validToTransferBalance = true;

      Account *fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      uint64_t createGasPenalty = std::max(
          CONTRACT_CREATE_GAS, (unsigned int)(transaction.GetCode().size() +
                                              transaction.GetData().size()));

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (transaction.GetGasLimitZil() < createGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimitZil()
                                          << " less than " << createGasPenalty);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      if (fromAccount->GetBalance() < gasDeposit) {
        LOG_GENERAL(
            WARNING,
            "The account doesn't have enough gas to create a contract. Bal: "
                << fromAccount->GetBalance()
                << " required: " << gasDeposit + amount);
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }
      // Check if the sender has enough balance to pay gasDeposit and transfer
      // amount
      /*else if (fromAccount->GetBalance() < gasDeposit + amount) {
        LOG_GENERAL(
            WARNING,
            "The account (balance: "
                << fromAccount->GetBalance()
                << ") "
                   "has enough balance to pay the gas price to deposit ("
                << gasDeposit
                << ") "
                   "but not enough for transfer the amount ("
                << amount
                << "), "
                   "create contract first and ignore amount "
                   "transfer however");
        validToTransferBalance = false;
      }*/

      // deduct scilla checker invoke gas
      if (gasRemained < SCILLA_CHECKER_INVOKE_GAS) {
        LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla checker");
        error_code = TxnStatus::INSUFFICIENT_GAS;
        return false;
      } else {
        gasRemained -= SCILLA_CHECKER_INVOKE_GAS;
      }

      // generate address for new contract account (note Eth accounting
      // difference here)
      toAddr =
          Account::GetAddressForContract(fromAddr, fromAccount->GetNonce(),
                                         transaction.GetVersionIdentifier());
      // instantiate the object for contract account
      // ** Remeber to call RemoveAccount if deployment failed halfway
      if (!this->AddAccount(toAddr, {0, 0})) {
        LOG_GENERAL(WARNING,
                    "AddAccount failed for contract address " << toAddr.hex());
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }
      Account *toAccount = this->GetAccount(toAddr);
      if (toAccount == nullptr) {
        LOG_GENERAL(WARNING, "toAccount is null ptr");
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }

      bool init = true;
      bool is_library = false;
      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      uint32_t scilla_version;

      try {
        // Initiate the contract account, including setting the contract code
        // store the immutable states
        if (!toAccount->InitContract(transaction.GetCode(),
                                     transaction.GetData(), toAddr, blockNum)) {
          LOG_GENERAL(WARNING, "InitContract failed");
          init = false;
        }
        std::vector<Address> extlibs;
        if (!toAccount->GetContractAuxiliaries(is_library, scilla_version,
                                               extlibs)) {
          LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
          this->RemoveAccount(toAddr);
          error_code = TxnStatus::FAIL_SCILLA_LIB;
          return false;
        }

        if (DISABLE_SCILLA_LIB && is_library) {
          LOG_GENERAL(WARNING, "ScillaLib disabled");
          this->RemoveAccount(toAddr);
          error_code = TxnStatus::FAIL_SCILLA_LIB;
          return false;
        }

        if (!PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
          LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
          this->RemoveAccount(toAddr);
          error_code = TxnStatus::FAIL_SCILLA_LIB;
          return false;
        }

        m_curBlockNum = blockNum;
        if (init &&
            !ExportCreateContractFiles(*toAccount, is_library, scilla_version,
                                       extlibs_exports)) {
          LOG_GENERAL(WARNING, "ExportCreateContractFiles failed");
          init = false;
        }

        if (init && !this->DecreaseBalance(fromAddr, gasDeposit)) {
          init = false;
        }
      } catch (const std::exception &e) {
        LOG_GENERAL(WARNING,
                    "Exception caught in create account (1): " << e.what());
        init = false;
      }

      if (!init) {
        this->RemoveAccount(toAddr);
        error_code = TxnStatus::FAIL_CONTRACT_INIT;
        return false;
      }

      // prepare IPC with current blockchain info provider.

      if (m_scillaIPCServer) {
        m_scillaIPCServer->setBCInfoProvider(
            m_curBlockNum, m_curDSBlockNum, m_originAddr, toAddr,
            toAccount->GetStorageRoot(), scilla_version);
      } else {
        LOG_GENERAL(
            WARNING,
            "Scilla IPC server is not setup correctly - detected null object");
      }

      // ************************************************************************
      // Undergo scilla checker
      bool ret_checker = true;
      std::string checkerPrint;

      InvokeInterpreter(CHECKER, checkerPrint, scilla_version, is_library,
                        gasRemained, 0, ret_checker, receipt);

      // 0xabc._version
      // 0xabc._depth.data1
      // 0xabc._type.data1

      std::map<std::string, zbytes> t_metadata;
      t_metadata.emplace(
          Contract::ContractStorage::GetContractStorage().GenerateStorageKey(
              toAddr, SCILLA_VERSION_INDICATOR, {}),
          DataConversion::StringToCharArray(std::to_string(scilla_version)));

      if (ret_checker &&
          !ParseContractCheckerOutput(toAddr, checkerPrint, receipt, t_metadata,
                                      gasRemained, is_library)) {
        ret_checker = false;
      }

      // *************************************************************************
      // Undergo scilla runner
      bool ret = true;

      if (ret_checker) {
        // deduct scilla runner invoke gas
        if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
          LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla runner");
          receipt.AddError(GAS_NOT_SUFFICIENT);
          ret = false;
        } else {
          gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
        }

        if (ret) {
          std::string runnerPrint;

          // invoke scilla runner
          InvokeInterpreter(RUNNER_CREATE, runnerPrint, scilla_version,
                            is_library, gasRemained, amount, ret, receipt);

          // parse runner output
          try {
            if (ret && !ParseCreateContract(gasRemained, runnerPrint, receipt,
                                            is_library)) {
              ret = false;
            }
            if (!ret) {
              gasRemained = std::min(
                  transaction.GetGasLimitZil() - createGasPenalty, gasRemained);
            }
          } catch (const std::exception &e) {
            LOG_GENERAL(WARNING,
                        "Exception caught in create account (2): " << e.what());
            ret = false;
          }
        }
      } else {
        gasRemained = std::min(transaction.GetGasLimitZil() - createGasPenalty,
                               gasRemained);
      }

      // *************************************************************************
      // Summary
      boost::multiprecision::uint128_t gasRefund;
      if (!SafeMath<boost::multiprecision::uint128_t>::mul(
              gasRemained, transaction.GetGasPriceQa(), gasRefund)) {
        this->RemoveAccount(toAddr);
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }
      if (!this->IncreaseBalance(fromAddr, gasRefund)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for gasRefund");
      }
      if (!ret || !ret_checker) {
        this->m_addressToAccount->erase(toAddr);

        receipt.SetResult(false);
        if (!ret) {
          receipt.AddError(RUNNER_FAILED);
        }
        if (!ret_checker) {
          receipt.AddError(CHECKER_FAILED);
        }
        receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemained);
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          this->RemoveAccount(toAddr);
          error_code = TxnStatus::MATH_ERROR;
          return false;
        }

        LOG_GENERAL(
            INFO,
            "Create contract failed, but return true in order to change state");

        if (LOG_SC) {
          LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
        }

        return true;  // Return true because the states already changed
      }

      if (transaction.GetGasLimitZil() < gasRemained) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimitZil()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      /*
      if (validToTransferBalance) {
        if (!this->TransferBalance(fromAddr, toAddr, amount)) {
          receipt.SetResult(false);
          receipt.AddError(BALANCE_TRANSFER_FAILED);
          receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
          receipt.update();

          if (!this->IncreaseNonce(fromAddr)) {
            this->RemoveAccount(toAddr);
            error_code = TxnStatus::MATH_ERROR;
            return false;
          }
          return true;
        }
      }
      */

      /// inserting address to create the uniqueness of the contract merkle trie
      t_metadata.emplace(Contract::ContractStorage::GenerateStorageKey(
                             toAddr, CONTRACT_ADDR_INDICATOR, {}),
                         toAddr.asBytes());

      if (!toAccount->UpdateStates(toAddr, t_metadata, {}, true)) {
        LOG_GENERAL(WARNING, "Account::UpdateStates failed");
        return false;
      }

      /// calculate total gas in receipt
      receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemained);

      if (is_library) {
        m_newLibrariesCreated.emplace_back(toAddr);
      }

      break;
    }
    case Transaction::CONTRACT_CALL: {
      // reset the storageroot update buffer atomic per transaction
      INC_STATUS(GetInvocationsCounter(), "Transaction",
                 "Contract-Call/Non Contract");
      m_storageRootUpdateBufferAtomic.clear();

      m_originAddr = fromAddr;

      Account *fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      LOG_GENERAL(INFO, "Call contract");

      uint64_t callGasPenalty = std::max(
          CONTRACT_INVOKE_GAS, (unsigned int)(transaction.GetData().size()));

      if (transaction.GetGasLimitZil() < callGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimitZil()
                                          << " less than " << callGasPenalty);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;

        return false;
      }

      if (fromAccount->GetBalance() < gasDeposit + amount) {
        LOG_GENERAL(WARNING, "The account (balance: "
                                 << fromAccount->GetBalance()
                                 << ") "
                                    "has not enough balance to deposit the gas "
                                    "price to deposit ("
                                 << gasDeposit
                                 << ") "
                                    "and transfer the amount ("
                                 << amount
                                 << ") in the txn, "
                                    "rejected");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }

      // deduct scilla checker invoke gas
      if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
        LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla runner");
        error_code = TxnStatus::INSUFFICIENT_GAS;
        return false;
      } else {
        gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
      }

      m_curSenderAddr = fromAddr;
      m_curEdges = 0;

      Account *toAccount = this->GetAccount(toAddr);
      if (toAccount == nullptr) {
        LOG_GENERAL(WARNING, "The target contract account doesn't exist");
        error_code = TxnStatus::INVALID_TO_ACCOUNT;
        return false;
      }

      bool is_library;
      uint32_t scilla_version;
      std::vector<Address> extlibs;
      if (!toAccount->GetContractAuxiliaries(is_library, scilla_version,
                                             extlibs)) {
        LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      if (is_library) {
        // Call to library should silently passed to scilla so that the gas is
        // charged based on the size of the message
        LOG_GENERAL(WARNING, "Library being called");
        // error_code = TxnStatus::FAIL_SCILLA_LIB;
        // return false;
      }

      if (DISABLE_SCILLA_LIB && !extlibs.empty()) {
        LOG_GENERAL(WARNING, "ScillaLib disabled");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      if (!PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
        LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      m_curBlockNum = blockNum;
      if (!ExportCallContractFiles(*toAccount, transaction, scilla_version,
                                   extlibs_exports)) {
        LOG_GENERAL(WARNING, "ExportCallContractFiles failed");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      DiscardAtomics();

      if (!this->DecreaseBalance(fromAddr, gasDeposit)) {
        LOG_GENERAL(WARNING, "DecreaseBalance failed");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      m_curGasLimit = transaction.GetGasLimitZil();
      m_curGasPrice = transaction.GetGasPriceQa();
      m_curContractAddr = toAddr;
      m_curAmount = amount;
      m_curNumShards = numShards;

      std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      // prepare IPC with current blockchain info provider.
      if (m_scillaIPCServer) {
        m_scillaIPCServer->setBCInfoProvider(
            m_curBlockNum, m_curDSBlockNum, m_originAddr, m_curContractAddr,
            toAccount->GetStorageRoot(), scilla_version);
      } else {
        LOG_GENERAL(WARNING, "m_scillaIPCServer not Initialised");
      }

      Contract::ContractStorage::GetContractStorage().BufferCurrentState();

      std::string runnerPrint;
      bool ret = true;

      InvokeInterpreter(RUNNER_CALL, runnerPrint, scilla_version, is_library,
                        gasRemained, this->GetBalance(toAddr), ret, receipt);

      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        LOG_GENERAL(INFO, "Executed root transition in " << r_timer_end(tpStart)
                                                         << " microseconds");
      }

      uint32_t tree_depth = 0;

      if (ret && !ParseCallContract(gasRemained, runnerPrint, receipt,
                                    tree_depth, scilla_version)) {
        receipt.RemoveAllTransitions();
        ret = false;
      }
      if (!ret) {
        Contract::ContractStorage::GetContractStorage().RevertPrevState();
        DiscardAtomics();
        gasRemained = std::min(transaction.GetGasLimitZil() - callGasPenalty,
                               gasRemained);
      } else {
        CommitAtomics();
      }
      boost::multiprecision::uint128_t gasRefund;
      if (!SafeMath<boost::multiprecision::uint128_t>::mul(
              gasRemained, transaction.GetGasPriceQa(), gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      if (!this->IncreaseBalance(fromAddr, gasRefund)) {
        LOG_GENERAL(WARNING, "IncreaseBalance failed for gasRefund");
      }

      if (transaction.GetGasLimitZil() < gasRemained) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimitZil()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemained);
      if (!ret) {
        receipt.SetResult(false);
        receipt.CleanEntry();
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
          return false;
        }

        LOG_GENERAL(
            INFO,
            "Call contract failed, but return true in order to change state");

        if (LOG_SC) {
          LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
        }

        return true;  // Return true because the states already changed
      }
      break;
    }
    default: {
      LOG_GENERAL(WARNING, "Txn is not typed correctly")
      error_code = TxnStatus::INCORRECT_TXN_TYPE;
      return false;
    }
  }

  if (!this->IncreaseNonce(fromAddr)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  receipt.SetResult(true);
  receipt.update();

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::CONTRACT_CALL: {
      /// since txn succeeded, commit the atomic buffer
      m_storageRootUpdateBuffer.insert(m_storageRootUpdateBufferAtomic.begin(),
                                       m_storageRootUpdateBufferAtomic.end());
      LOG_GENERAL(INFO, "Executing contract call transaction finished");
      break;
    }
    case Transaction::CONTRACT_CREATION: {
      LOG_GENERAL(INFO, "Executing contract creation transaction finished");
      break;
    }
    default:
      break;
  }

  if (LOG_SC) {
    LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
  }

  return true;
}

bool AccountStoreSC::PopulateExtlibsExports(
    uint32_t scilla_version, const std::vector<Address> &extlibs,
    std::map<Address, std::pair<std::string, std::string>> &extlibs_exports) {
  LOG_MARKER();
  std::function<bool(const std::vector<Address> &,
                     std::map<Address, std::pair<std::string, std::string>> &)>
      extlibsExporter;
  extlibsExporter = [this, &scilla_version, &extlibsExporter](
                        const std::vector<Address> &extlibs,
                        std::map<Address, std::pair<std::string, std::string>>
                            &extlibs_exports) -> bool {
    // export extlibs
    for (const auto &libAddr : extlibs) {
      if (extlibs_exports.find(libAddr) != extlibs_exports.end()) {
        continue;
      }

      Account *libAcc = this->GetAccount(libAddr);
      if (libAcc == nullptr) {
        LOG_GENERAL(WARNING, "libAcc: " << libAddr << " does not exist");
        return false;
      }

      /// Check whether there are caches
      std::string code_path = EXTLIB_FOLDER + '/' + libAddr.hex();
      code_path += LIBRARY_CODE_EXTENSION;
      std::string json_path = EXTLIB_FOLDER + '/' + libAddr.hex() + ".json";
      if (std::filesystem::exists(code_path) &&
          std::filesystem::exists(json_path)) {
        continue;
      }

      uint32_t ext_scilla_version;
      bool ext_is_lib = false;
      std::vector<Address> ext_extlibs;

      if (!libAcc->GetContractAuxiliaries(ext_is_lib, ext_scilla_version,
                                          ext_extlibs)) {
        LOG_GENERAL(WARNING,
                    "libAcc: " << libAddr << " GetContractAuxiliaries failed");
        return false;
      }

      if (!ext_is_lib) {
        LOG_GENERAL(WARNING, "libAcc: " << libAddr << " is not library");
        return false;
      }

      if (ext_scilla_version != scilla_version) {
        LOG_GENERAL(WARNING,
                    "libAcc: " << libAddr << " scilla version mismatch");
        return false;
      }

      extlibs_exports[libAddr] = {
          DataConversion::CharArrayToString(libAcc->GetCode()),
          DataConversion::CharArrayToString(libAcc->GetInitData())};

      if (!extlibsExporter(ext_extlibs, extlibs_exports)) {
        return false;
      }
    }

    return true;
  };
  return extlibsExporter(extlibs, extlibs_exports);
}

bool AccountStoreSC::ExportCreateContractFiles(
    const Account &contract, bool is_library, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();

  std::filesystem::remove_all("./" + SCILLA_FILES);
  std::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(std::filesystem::exists("./" + SCILLA_LOG))) {
    std::filesystem::create_directories("./" + SCILLA_LOG);
  }

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version, m_root_w_version)) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    // Scilla code
    std::ofstream os(INPUT_CODE + (is_library ? LIBRARY_CODE_EXTENSION
                                              : CONTRACT_FILE_EXTENSION));
    os << DataConversion::CharArrayToString(contract.GetCode());
    os.close();

    ExportCommonFiles(os, contract, extlibs_exports);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

void AccountStoreSC::ExportCommonFiles(
    std::ofstream &os, const Account &contract,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  os.open(INIT_JSON);
  if (LOG_SC) {
    LOG_GENERAL(
        INFO, "init data to export: "
                  << DataConversion::CharArrayToString(contract.GetInitData()));
  }
  os << DataConversion::CharArrayToString(contract.GetInitData());
  os.close();

  for (const auto &extlib_export : extlibs_exports) {
    std::string code_path =
        EXTLIB_FOLDER + '/' + "0x" + extlib_export.first.hex();
    code_path += LIBRARY_CODE_EXTENSION;
    std::filesystem::remove(code_path);

    os.open(code_path);
    os << extlib_export.second.first;
    os.close();

    std::string init_path =
        EXTLIB_FOLDER + '/' + "0x" + extlib_export.first.hex() + ".json";
    std::filesystem::remove(init_path);

    os.open(init_path);
    os << extlib_export.second.second;
    os.close();
  }
}

bool AccountStoreSC::ExportContractFiles(
    Account &contract, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();
  std::chrono::system_clock::time_point tpStart;

  std::filesystem::remove_all("./" + SCILLA_FILES);
  std::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(std::filesystem::exists("./" + SCILLA_LOG))) {
    std::filesystem::create_directories("./" + SCILLA_LOG);
  }

  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version, m_root_w_version)) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    std::string scillaCodeExtension = CONTRACT_FILE_EXTENSION;
    if (contract.IsLibrary()) {
      scillaCodeExtension = LIBRARY_CODE_EXTENSION;
    }
    CreateScillaCodeFiles(contract, extlibs_exports, scillaCodeExtension);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(INFO, "LDB Read (microsec) = " << r_timer_end(tpStart));
  }

  return true;
}

void AccountStoreSC::CreateScillaCodeFiles(
    Account &contract,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports,
    const std::string &scillaCodeExtension) {
  LOG_MARKER();
  // Scilla code
  std::ofstream os(INPUT_CODE + scillaCodeExtension);
  os << DataConversion::CharArrayToString(contract.GetCode());
  os.close();

  ExportCommonFiles(os, contract, extlibs_exports);
}

bool AccountStoreSC::ExportCallContractFiles(
    Account &contract, const Transaction &transaction, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();

  if (!ExportContractFiles(contract, scilla_version, extlibs_exports)) {
    LOG_GENERAL(WARNING, "ExportContractFiles failed");
    return false;
  }

  try {
    // Message Json
    std::string dataStr(transaction.GetData().begin(),
                        transaction.GetData().end());
    Json::Value msgObj;
    if (!JSONUtils::GetInstance().convertStrtoJson(dataStr, msgObj)) {
      return false;
    }
    std::string prepend = "0x";
    msgObj["_sender"] =
        prepend +
        Account::GetAddressFromPublicKey(transaction.GetSenderPubKey()).hex();
    msgObj["_origin"] = prepend + m_originAddr.hex();
    msgObj["_amount"] = transaction.GetAmountQa().convert_to<std::string>();

    JSONUtils::GetInstance().writeJsontoFile(INPUT_MESSAGE_JSON, msgObj);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

bool AccountStoreSC::ExportCallContractFiles(
    Account &contract, const Json::Value &contractData, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();

  if (!ExportContractFiles(contract, scilla_version, extlibs_exports)) {
    LOG_GENERAL(WARNING, "ExportContractFiles failed");
    return false;
  }

  try {
    JSONUtils::GetInstance().writeJsontoFile(INPUT_MESSAGE_JSON, contractData);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

bool AccountStoreSC::ParseContractCheckerOutput(
    const Address &addr, const std::string &checkerPrint,
    TransactionReceipt &receipt, std::map<std::string, zbytes> &metadata,
    uint64_t &gasRemained, bool is_library) {
  LOG_MARKER();

  LOG_GENERAL(
      INFO,
      "Output: " << std::endl
                 << (checkerPrint.length() > MAX_SCILLA_OUTPUT_SIZE_IN_BYTES
                         ? checkerPrint.substr(
                               0, MAX_SCILLA_OUTPUT_SIZE_IN_BYTES) +
                               "\n ... "
                         : checkerPrint));

  Json::Value root;
  try {
    if (!JSONUtils::GetInstance().convertStrtoJson(checkerPrint, root)) {
      receipt.AddError(JSON_OUTPUT_CORRUPTED);
      return false;
    }

    if (!root.isMember("gas_remaining")) {
      LOG_GENERAL(
          WARNING,
          "The json output of this contract didn't contain gas_remaining");
      if (gasRemained > CONTRACT_CREATE_GAS) {
        gasRemained -= CONTRACT_CREATE_GAS;
      } else {
        gasRemained = 0;
      }
      receipt.AddError(NO_GAS_REMAINING_FOUND);
      return false;
    }
    try {
      gasRemained = std::min(
          gasRemained,
          boost::lexical_cast<uint64_t>(root["gas_remaining"].asString()));
    } catch (...) {
      LOG_GENERAL(WARNING, "_amount " << root["gas_remaining"].asString()
                                      << " is not numeric");
      return false;
    }
    LOG_GENERAL(INFO, "gasRemained: " << gasRemained);

    if (is_library) {
      if (root.isMember("errors")) {
        receipt.AddException(root["errors"]);
        return false;
      }
    } else {
      if (!root.isMember("contract_info")) {
        receipt.AddError(CHECKER_FAILED);

        if (root.isMember("errors")) {
          receipt.AddException(root["errors"]);
        }
        return false;
      }
      bool hasMap = false;

      auto handleTypeForStateVar = [&](const Json::Value &stateVars) {
        if (!stateVars.isArray()) {
          LOG_GENERAL(WARNING, "An array of state variables expected."
                                   << stateVars.toStyledString());
          return false;
        }
        for (const auto &field : stateVars) {
          if (field.isMember("vname") && field.isMember("depth") &&
              field["depth"].isNumeric() && field.isMember("type")) {
            metadata.emplace(
                Contract::ContractStorage::GetContractStorage()
                    .GenerateStorageKey(addr, MAP_DEPTH_INDICATOR,
                                        {field["vname"].asString()}),
                DataConversion::StringToCharArray(field["depth"].asString()));
            if (!hasMap && field["depth"].asInt() > 0) {
              hasMap = true;
            }
            metadata.emplace(
                Contract::ContractStorage::GetContractStorage()
                    .GenerateStorageKey(addr, TYPE_INDICATOR,
                                        {field["vname"].asString()}),
                DataConversion::StringToCharArray(field["type"].asString()));
          } else {
            LOG_GENERAL(WARNING,
                        "Unexpected field detected" << field.toStyledString());
            return false;
          }
        }
        return true;
      };
      if (root["contract_info"].isMember("fields")) {
        if (!handleTypeForStateVar(root["contract_info"]["fields"])) {
          return false;
        }
      }
    }
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what() << " checkerPrint: "
                                              << checkerPrint);
    return false;
  }

  return true;
}

bool AccountStoreSC::ParseCreateContract(uint64_t &gasRemained,
                                         const std::string &runnerPrint,
                                         TransactionReceipt &receipt,
                                         bool is_library) {
  Json::Value jsonOutput;
  if (!ParseCreateContractOutput(jsonOutput, runnerPrint, receipt)) {
    return false;
  }
  return ParseCreateContractJsonOutput(jsonOutput, gasRemained, receipt,
                                       is_library);
}

bool AccountStoreSC::ParseCreateContractOutput(Json::Value &jsonOutput,
                                               const std::string &runnerPrint,
                                               TransactionReceipt &receipt) {
  // LOG_MARKER();

  if (LOG_SC) {
    LOG_GENERAL(
        INFO,
        "Output: " << std::endl
                   << (runnerPrint.length() > MAX_SCILLA_OUTPUT_SIZE_IN_BYTES
                           ? runnerPrint.substr(
                                 0, MAX_SCILLA_OUTPUT_SIZE_IN_BYTES) +
                                 "\n ... "
                           : runnerPrint));
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(runnerPrint, jsonOutput)) {
    receipt.AddError(JSON_OUTPUT_CORRUPTED);

    return false;
  }
  return true;
}

bool AccountStoreSC::ParseCreateContractJsonOutput(const Json::Value &_json,
                                                   uint64_t &gasRemained,
                                                   TransactionReceipt &receipt,
                                                   bool is_library) {
  // LOG_MARKER();
  if (!_json.isMember("gas_remaining")) {
    LOG_GENERAL(
        WARNING,
        "The json output of this contract didn't contain gas_remaining");
    if (gasRemained > CONTRACT_CREATE_GAS) {
      gasRemained -= CONTRACT_CREATE_GAS;
    } else {
      gasRemained = 0;
    }
    receipt.AddError(NO_GAS_REMAINING_FOUND);
    return false;
  }
  try {
    gasRemained = std::min(gasRemained, boost::lexical_cast<uint64_t>(
                                            _json["gas_remaining"].asString()));
  } catch (...) {
    LOG_GENERAL(WARNING, "_amount " << _json["gas_remaining"].asString()
                                    << " is not numeric");
    return false;
  }
  LOG_GENERAL(INFO, "gasRemained: " << gasRemained);

  if (!is_library) {
    if (!_json.isMember("messages") || !_json.isMember("events")) {
      if (_json.isMember("errors")) {
        LOG_GENERAL(WARNING, "Contract creation failed");
        receipt.AddError(CREATE_CONTRACT_FAILED);
        receipt.AddException(_json["errors"]);
      } else {
        LOG_GENERAL(WARNING, "JSON output of this contract is corrupted");
        receipt.AddError(OUTPUT_ILLEGAL);
      }
      return false;
    }

    if (_json["messages"].type() == Json::nullValue &&
        _json["states"].type() == Json::arrayValue &&
        _json["events"].type() == Json::arrayValue) {
      return true;
    }

    LOG_GENERAL(WARNING,
                "Didn't get desired json output from the interpreter for "
                "create contract");
    receipt.AddError(OUTPUT_ILLEGAL);
    return false;
  }

  return true;
}

bool AccountStoreSC::ParseCallContract(uint64_t &gasRemained,
                                       const std::string &runnerPrint,
                                       TransactionReceipt &receipt,
                                       uint32_t tree_depth,
                                       uint32_t scilla_version) {
  Json::Value jsonOutput;
  if (!ParseCallContractOutput(jsonOutput, runnerPrint, receipt)) {
    return false;
  }
  return ParseCallContractJsonOutput(jsonOutput, gasRemained, receipt,
                                     tree_depth, scilla_version);
}

bool AccountStoreSC::ParseCallContractOutput(Json::Value &jsonOutput,
                                             const std::string &runnerPrint,
                                             TransactionReceipt &receipt) {
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (LOG_SC) {
    LOG_GENERAL(
        INFO,
        "Output: " << std::endl
                   << (runnerPrint.length() > MAX_SCILLA_OUTPUT_SIZE_IN_BYTES
                           ? runnerPrint.substr(
                                 0, MAX_SCILLA_OUTPUT_SIZE_IN_BYTES) +
                                 "\n ... "
                           : runnerPrint));
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(runnerPrint, jsonOutput)) {
    receipt.AddError(JSON_OUTPUT_CORRUPTED);
    return false;
  }
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(INFO, "Parse scilla-runner output (microseconds) = "
                          << r_timer_end(tpStart));
  }

  return true;
}

bool AccountStoreSC::ParseCallContractJsonOutput(const Json::Value &_json,
                                                 uint64_t &gasRemained,
                                                 TransactionReceipt &receipt,
                                                 uint32_t tree_depth,
                                                 uint32_t pre_scilla_version) {
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (!_json.isMember("gas_remaining")) {
    LOG_GENERAL(
        WARNING,
        "The json output of this contract didn't contain gas_remaining");
    if (gasRemained > CONTRACT_INVOKE_GAS) {
      gasRemained -= CONTRACT_INVOKE_GAS;
    } else {
      gasRemained = 0;
    }
    receipt.AddError(NO_GAS_REMAINING_FOUND);
    return false;
  }
  uint64_t startGas = gasRemained;
  try {
    gasRemained = std::min(gasRemained, boost::lexical_cast<uint64_t>(
                                            _json["gas_remaining"].asString()));
  } catch (...) {
    LOG_GENERAL(WARNING, "_amount " << _json["gas_remaining"].asString()
                                    << " is not numeric");
    return false;
  }
  LOG_GENERAL(INFO, "gasRemained: " << gasRemained);

  if (!_json.isMember("messages") || !_json.isMember("events")) {
    if (_json.isMember("errors")) {
      LOG_GENERAL(WARNING, "Call contract failed");
      receipt.AddError(CALL_CONTRACT_FAILED);
      receipt.AddException(_json["errors"]);
    } else {
      LOG_GENERAL(WARNING, "JSON output of this contract is corrupted");
      receipt.AddError(OUTPUT_ILLEGAL);
    }
    return false;
  }

  if (!_json.isMember("_accepted")) {
    LOG_GENERAL(WARNING,
                "The json output of this contract doesn't contain _accepted");
    receipt.AddError(NO_ACCEPTED_FOUND);
    return false;
  }

  bool accepted = (_json["_accepted"].asString() == "true");
  if (accepted) {
    // LOG_GENERAL(INFO, "Contract accept amount transfer");
    if (!TransferBalanceAtomic(m_curSenderAddr, m_curContractAddr,
                               m_curAmount)) {
      LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
      receipt.AddError(BALANCE_TRANSFER_FAILED);
      return false;
    }
  } else {
    LOG_GENERAL(WARNING, "Contract refuse amount transfer");
  }

  if (tree_depth == 0) {
    // first call in a txn
    receipt.AddAccepted(accepted);
  } else {
    if (!receipt.AddAcceptedForLastTransition(accepted)) {
      LOG_GENERAL(WARNING, "AddAcceptedForLastTransition failed");
      return false;
    }
  }

  Account *contractAccount =
      m_accountStoreAtomic->GetAccount(m_curContractAddr);
  if (contractAccount == nullptr) {
    LOG_GENERAL(WARNING, "contractAccount is null ptr");
    receipt.AddError(CONTRACT_NOT_EXIST);
    return false;
  }

  try {
    for (const auto &e : _json["events"]) {
      LogEntry entry;
      if (!entry.Install(e, m_curContractAddr)) {
        receipt.AddError(LOG_ENTRY_INSTALL_FAILED);
        return false;
      }
      receipt.AddLogEntry(entry);
    }
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  bool ret = false;

  if (_json["messages"].type() != Json::arrayValue) {
    LOG_GENERAL(INFO, "messages is not in array value");
    return false;
  }

  // If output message is null
  if (_json["messages"].empty()) {
    LOG_GENERAL(INFO,
                "empty message in scilla output when invoking a "
                "contract, transaction finished");
    m_storageRootUpdateBufferAtomic.emplace(m_curContractAddr);
    ret = true;
  }

  Address recipient;
  Account *account = nullptr;

  if (!ret) {
    // Buffer the Addr for current caller
    Address curContractAddr = m_curContractAddr;
    for (const auto &msg : _json["messages"]) {
      LOG_GENERAL(INFO, "Process new message");

      // a buffer for `ret` flag to be reset per loop
      bool t_ret = ret;

      // Non-null messages must have few mandatory fields.
      if (!msg.isMember("_tag") || !msg.isMember("_amount") ||
          !msg.isMember("params") || !msg.isMember("_recipient")) {
        LOG_GENERAL(
            WARNING,
            "The message in the json output of this contract is corrupted");
        receipt.AddError(MESSAGE_CORRUPTED);
        return false;
      }

      try {
        m_curAmount = boost::lexical_cast<uint128_t>(msg["_amount"].asString());
      } catch (...) {
        LOG_GENERAL(WARNING, "_amount " << msg["_amount"].asString()
                                        << " is not numeric");
        return false;
      }

      recipient = Address(msg["_recipient"].asString());
      if (IsNullAddress(recipient)) {
        LOG_GENERAL(WARNING, "The recipient can't be null address");
        receipt.AddError(RECEIPT_IS_NULL);
        return false;
      }

      account = m_accountStoreAtomic->GetAccount(recipient);

      if (account == nullptr) {
        AccountStoreBase::AddAccount(recipient, {0, 0});
        account = m_accountStoreAtomic->GetAccount(recipient);
      }

      // Recipient is non-contract
      if (!account->isContract()) {
        LOG_GENERAL(INFO, "The recipient is non-contract");
        if (!TransferBalanceAtomic(curContractAddr, recipient, m_curAmount)) {
          receipt.AddError(BALANCE_TRANSFER_FAILED);
          return false;
        } else {
          t_ret = true;
        }
      }

      // Recipient is contract
      // _tag field is empty
      if (msg["_tag"].asString().empty()) {
        LOG_GENERAL(INFO,
                    "_tag in the scilla output is empty when invoking a "
                    "contract, transaction finished");
        t_ret = true;
      }

      m_storageRootUpdateBufferAtomic.emplace(curContractAddr);
      receipt.AddTransition(curContractAddr, msg, tree_depth);

      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        LOG_GENERAL(INFO,
                    "LDB Write (microseconds) = " << r_timer_end(tpStart));
        LOG_GENERAL(INFO, "Gas used = " << (startGas - gasRemained));
      }

      if (t_ret) {
        // return true;
        continue;
      }

      LOG_GENERAL(INFO, "Call another contract in chain");
      receipt.AddEdge();
      ++m_curEdges;

      // deduct scilla runner invoke gas
      if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
        LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla runner");
        receipt.AddError(GAS_NOT_SUFFICIENT);
        return false;
      } else {
        gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
      }

      if (m_curEdges > MAX_CONTRACT_EDGES) {
        LOG_GENERAL(
            WARNING,
            "maximum contract edges reached, cannot call another contract");
        receipt.AddError(MAX_EDGES_REACHED);
        return false;
      }

      Json::Value input_message;
      input_message["_sender"] = "0x" + curContractAddr.hex();
      input_message["_origin"] = "0x" + m_originAddr.hex();
      input_message["_amount"] = msg["_amount"];
      input_message["_tag"] = msg["_tag"];
      input_message["params"] = msg["params"];

      if (account == nullptr) {
        LOG_GENERAL(WARNING, "account still null");
        receipt.AddError(INTERNAL_ERROR);
        return false;
      }

      // prepare IPC with the recipient contract address
      bool is_library;
      std::vector<Address> extlibs;
      uint32_t scilla_version;

      if (!account->GetContractAuxiliaries(is_library, scilla_version,
                                           extlibs)) {
        LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
        receipt.AddError(INTERNAL_ERROR);
        return false;
      }

      // prepare IPC with current blockchain info provider.
      m_scillaIPCServer->setBCInfoProvider(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, recipient,
          account->GetStorageRoot(), scilla_version);

      if (DISABLE_SCILLA_LIB && !extlibs.empty()) {
        LOG_GENERAL(WARNING, "ScillaLib disabled");
        return false;
      }

      if (scilla_version != pre_scilla_version) {
        LOG_GENERAL(WARNING, "Scilla version inconsistent");
        receipt.AddError(VERSION_INCONSISTENT);
        return false;
      }

      if (is_library) {
        // Scilla should be invoked for message sent to library so that the GAS
        // is charged
        LOG_GENERAL(WARNING, "Library being called");
        // receipt.AddError(LIBRARY_AS_RECIPIENT);
        // return false;
      }

      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      if (!PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
        LOG_GENERAL(WARNING, "PopulateExtlibsExports");
        receipt.AddError(LIBRARY_EXTRACTION_FAILED);
        return false;
      }

      if (!ExportCallContractFiles(*account, input_message, scilla_version,
                                   extlibs_exports)) {
        LOG_GENERAL(WARNING, "ExportCallContractFiles failed");
        receipt.AddError(PREPARATION_FAILED);
        return false;
      }

      // prepare IPC with current blockchain info provider.
      m_scillaIPCServer->setBCInfoProvider(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, recipient,
          account->GetStorageRoot(), scilla_version);

      std::string runnerPrint;
      bool result = true;

      InvokeInterpreter(RUNNER_CALL, runnerPrint, scilla_version, is_library,
                        gasRemained, account->GetBalance(), result, receipt);

      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        LOG_GENERAL(INFO, "Executed " << input_message["_tag"] << " in "
                                      << r_timer_end(tpStart)
                                      << " microseconds");
      }

      if (!result) {
        return false;
      }

      m_curSenderAddr = curContractAddr;
      m_curContractAddr = recipient;
      if (!ParseCallContract(gasRemained, runnerPrint, receipt, tree_depth + 1,
                             scilla_version)) {
        LOG_GENERAL(WARNING, "ParseCallContract failed of calling contract: "
                                 << recipient);
        return false;
      }

      if (!this->IncreaseNonce(curContractAddr)) {
        return false;
      }
    }
  }

  return true;
}

void AccountStoreSC::ProcessStorageRootUpdateBuffer() {
  LOG_MARKER();
  {
    std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
    for (const auto &addr : m_storageRootUpdateBuffer) {
      Account *account = this->GetAccount(addr);
      if (account == nullptr) {
        continue;
      }
      LOG_GENERAL(INFO, "Address: " << addr.hex());

      // *** IMPORTANT ***
      // Setting storageRoot to empty to represent the states get changed
      account->SetStorageRoot(dev::h256());
    }
  }
  CleanStorageRootUpdateBuffer();
}

void AccountStoreSC::CleanStorageRootUpdateBuffer() {
  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_storageRootUpdateBuffer.clear();
}

bool AccountStoreSC::TransferBalanceAtomic(const Address &from,
                                           const Address &to,
                                           const uint128_t &delta) {
  // LOG_MARKER();
  LOG_GENERAL(WARNING,
              "AccountStoreSC::TransferBalanceAtomicTransferBalanceAtomic from "
                  << from << ", to: " << to << ", value: " << delta);
  return m_accountStoreAtomic->TransferBalance(from, to, delta);
}

void AccountStoreSC::CommitAtomics() {
  LOG_MARKER();
  for (const auto &entry : *m_accountStoreAtomic->GetAddressToAccount()) {
    Account *account = this->GetAccount(entry.first);
    if (account != nullptr) {
      *account = entry.second;
    } else {
      // this->m_addressToAccount.emplace(std::make_pair(entry.first,
      // entry.second));
      this->AddAccount(entry.first, entry.second);
    }
  }
}

void AccountStoreSC::DiscardAtomics() {
  LOG_MARKER();
  m_accountStoreAtomic->Init();
}

void AccountStoreSC::NotifyTimeout() {
  LOG_MARKER();
  m_txnProcessTimeout = true;
  m_CallContractConditionVariable.notify_all();
}

Account *AccountStoreSC::GetAccountAtomic(const dev::h160 &addr) {
  return m_accountStoreAtomic->GetAccount(addr);
}

void AccountStoreSC::SetScillaIPCServer(
    std::shared_ptr<ScillaIPCServer> scillaIPCServer) {
  LOG_MARKER();
  m_scillaIPCServer = std::move(scillaIPCServer);
}

void AccountStoreSC::CleanNewLibrariesCache() {
  for (const auto &addr : m_newLibrariesCreated) {
    std::filesystem::remove(addr.hex() + LIBRARY_CODE_EXTENSION);
    std::filesystem::remove(addr.hex() + ".json");
  }
  m_newLibrariesCreated.clear();
}
