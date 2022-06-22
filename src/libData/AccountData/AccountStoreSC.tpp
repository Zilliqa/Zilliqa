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

#include <boost/filesystem.hpp>
#include <chrono>

#include <vector>
#include "EvmClient.h"
#include "ScillaClient.h"
#include "libPersistence/ContractStorage.h"
#include "libServer/ScillaIPCServer.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/SafeMath.h"
#include "libUtils/ScillaUtils.h"
#include "libUtils/SysCommand.h"

// 5mb
const unsigned int MAX_SCILLA_OUTPUT_SIZE_IN_BYTES = 5120;

template <class MAP>
AccountStoreSC<MAP>::AccountStoreSC() {
  m_accountStoreAtomic = std::make_unique<AccountStoreAtomic<MAP>>(*this);
  m_txnProcessTimeout = false;
}

template <class MAP>
void AccountStoreSC<MAP>::Init() {
  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  AccountStoreBase<MAP>::Init();
  m_curContractAddr.clear();
  m_curSenderAddr.clear();
  m_curAmount = 0;
  m_curGasLimit = 0;
  m_curGasPrice = 0;
  m_txnProcessTimeout = false;

  boost::filesystem::remove_all(EXTLIB_FOLDER);
  boost::filesystem::create_directories(EXTLIB_FOLDER);
}

template <class MAP>
void AccountStoreSC<MAP>::InvokeInterpreter(
    INVOKE_TYPE invoke_type, std::string& interprinterPrint,
    const uint32_t& version, bool is_library, const uint64_t& available_gas,
    const boost::multiprecision::uint128_t& balance, bool& ret,
    TransactionReceipt& receipt) {
  bool call_already_finished = false;
  auto func = [this, &interprinterPrint, &invoke_type, &version, &is_library,
               &available_gas, &balance, &ret, &receipt,
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
                ScillaUtils::GetCallContractJson(m_root_w_version,
                                                 available_gas, balance),
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
    cv_callContract.notify_all();
  };
  DetachedFunction(1, func);

  {
    std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
    if (!call_already_finished) {
      cv_callContract.wait(lk);
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
}

template <class MAP>
void AccountStoreSC<MAP>::EvmCallRunner(
    INVOKE_TYPE invoke_type, EvmCallParameters& params, const uint32_t& version,
    bool& ret, TransactionReceipt& receipt,
    evmproj::CallResponse& evmReturnValues) {
  bool call_already_finished = false;

  auto worker = [this, &params, &invoke_type, &ret, &receipt, &version,
                 &call_already_finished, &evmReturnValues]() mutable -> void {
    Json::Value jval;
    if (invoke_type == RUNNER_CREATE || invoke_type == RUNNER_CALL) {
      ret = EvmClient::GetInstance().CallRunner(
          version, EvmUtils::GetEvmCallJson(params), evmReturnValues);
    }

    call_already_finished = true;
    cv_callContract.notify_all();
  };
  // Run the Lambda on a detached thread and then wait for it to complete.
  // means it cannot crash us if it dies.
  DetachedFunction(1, worker);
  // Wait for the worker to finish
  {
    std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
    if (!call_already_finished) {
      cv_callContract.wait(lk);
    } else {
      LOG_GENERAL(INFO, "Call functions already finished!");
    }
  }
  // not sure how timeout can be set - investigate
  if (m_txnProcessTimeout) {
    LOG_GENERAL(WARNING, "Txn processing timeout!");

    EvmClient::GetInstance().CheckClient(0, true);
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    ret = false;
  }
}

// New Invoker for EVM returns the gas, maybe change this
template <class MAP>
uint64_t AccountStoreSC<MAP>::InvokeEvmInterpreter(
    Account* contractAccount, INVOKE_TYPE invoke_type,
    EvmCallParameters& params, const uint32_t& version, bool& ret,
    TransactionReceipt& receipt, evmproj::CallResponse& evmReturnValues) {
  EvmCallRunner(invoke_type, params, version, ret, receipt, evmReturnValues);
  uint64_t gas = params.m_available_gas;
  if (not evmReturnValues.isSuccess()) {
    LOG_GENERAL(WARNING, evmReturnValues.ExitReason());
  }
  // switch ret to reflect our overall success
  ret = evmReturnValues.isSuccess() ? ret : false;

  if (!evmReturnValues.Logs().empty()) {
    Json::Value _json = Json::arrayValue;
    bool success = true;
    for (const auto& lval : evmReturnValues.Logs()) {
      LOG_GENERAL(INFO, "Logs: " << lval);
      Json::Reader _reader;
      Json::Value tmp;
      try {
        success = _reader.parse(lval, tmp);
      } catch (std::exception& e) {
        LOG_GENERAL(WARNING, "Exception: " << e.what());
      }
      if (!success) {
        LOG_GENERAL(WARNING, "Parsing json unsuccessful " << lval)
        break;
      }
      _json.append(tmp);
    }
    receipt.AddJsonEntry(_json);
  }
  gas = EvmUtils::UpdateGasRemaining(receipt, invoke_type, gas,
                                     evmReturnValues.Gas());
  std::map<std::string, bytes> states;
  std::vector<std::string> toDeletes;
  // parse the return values from the call to evm.
  for (const auto& it : evmReturnValues.m_apply) {
    if (it->OperationType() == "delete") {
      // be careful with this call needs further testing
      this->RemoveAccount(Address(it->Address()));
    } else {
      // Get the account that this apply instruction applies to
      Account* targetAccount = this->GetAccount(Address(it->Address()));
      if (targetAccount == nullptr) {
        LOG_GENERAL(
            WARNING,
            "Cannot find account for address given in  Apply operation from "
            "EVM-DS"
            " ");
        return gas;
      }
      if (it->OperationType() == "modify") {
        try {
          if (it->isResetStorage()) {
            states.clear();
            toDeletes.clear();

            Contract::ContractStorage::GetContractStorage()
                .FetchStateDataForContract(states, Address(it->Address()), "",
                                           {}, true);
            for (const auto& x : states) {
              toDeletes.emplace_back(x.first);
            }
            if (!targetAccount->UpdateStates(Address(it->Address()), {},
                                             toDeletes, true)) {
              LOG_GENERAL(
                  WARNING,
                  "Failed to update states hby setting indices for deletion "
                  "for "
                      << it->Address());
            }
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will exmine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to reset storage " << e.what());
        }
        // If Instructed to reset the Code do so and call SetImmutable to reset
        // the hash
        try {
          if (it->hasCode() && it->Code().size() > 0)
            targetAccount->SetImmutable(
                DataConversion::StringToCharArray("EVM" + it->Code()),
                contractAccount->GetInitData());
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will exmine exact possibilities and catch specific exceptions.
          LOG_GENERAL(
              WARNING,
              "Exception thrown trying to update Contract code " << e.what());
        }
        // Actually Update the state for the contract
        try {
          for (const auto& sit : it->Storage()) {
            if (not Contract::ContractStorage::GetContractStorage()
                        .UpdateStateValue(
                            Address(it->Address()),
                            DataConversion::StringToCharArray(sit.Key()), 0,
                            DataConversion::StringToCharArray(sit.Value()),
                            0)) {
              LOG_GENERAL(WARNING,
                          "Exception thrown trying to update state in Contract "
                          "storage "
                              << it->Address());
            }
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will exmine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to update state on the contract "
                          << e.what());
        }
        try {
          if (it->hasBalance() && it->Balance().size())
            targetAccount->SetBalance(uint128_t(it->Balance()));
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will exmine exact possibilities and catch specific exceptions.
          LOG_GENERAL(
              WARNING,
              "Exception thrown trying to update balance on target Account "
                  << e.what());
        }
        try {
          if (it->hasNonce() && it->Nonce().size())
            targetAccount->SetNonce(std::stoull(it->Nonce()));
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will exmine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to set Nonce on target Account "
                          << e.what());
        }
        // Mark the Address as updated
        m_storageRootUpdateBufferAtomic.emplace(it->Address());
      }
    }
  }

  if (invoke_type == RUNNER_CREATE)
    contractAccount->SetImmutable(DataConversion::StringToCharArray(
                                      "EVM" + evmReturnValues.ReturnedBytes()),
                                  contractAccount->GetInitData());
  return gas;
}

template <class MAP>
bool AccountStoreSC<MAP>::ViewAccounts(EvmCallParameters& params, bool& ret,
                                       std::string& result) {
  TransactionReceipt rcpt;
  uint32_t evm_version{0};
  evmproj::CallResponse response;
  EvmCallRunner(RUNNER_CALL, params, evm_version, ret, rcpt, response);
  result = response.m_return;
  if (LOG_SC) {
    LOG_GENERAL(INFO, response);
  }

  return ret;
}

template <class MAP>
bool AccountStoreSC<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                         const unsigned int& numShards,
                                         const bool& isDS,
                                         const Transaction& transaction,
                                         TransactionReceipt& receipt,
                                         TxnStatus& error_code) {
  LOG_MARKER();

  if (LOG_SC) LOG_GENERAL(INFO, "Process txn: " << transaction.GetTranID());

  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_curIsDS = isDS;
  m_txnProcessTimeout = false;
  bool isScilla{true};

  error_code = TxnStatus::NOT_PRESENT;

  const PubKey& senderPubKey = transaction.GetSenderPubKey();
  const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);

  // Initiate gasRemained
  uint64_t gasRemained = transaction.GetGasLimit();

  // Get the amount of deposit for running this txn
  uint128_t gasDeposit;
  if (!SafeMath<uint128_t>::mul(gasRemained, transaction.GetGasPrice(),
                                gasDeposit)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::NON_CONTRACT: {
      // LOG_GENERAL(INFO, "Normal transaction");

      // Disallow normal transaction to contract account
      Account* toAccount = this->GetAccount(transaction.GetToAddr());
      if (toAccount != nullptr) {
        if (toAccount->isContract()) {
          LOG_GENERAL(WARNING, "Contract account won't accept normal txn");
          error_code = TxnStatus::INVALID_TO_ACCOUNT;
          return false;
        }
      }

      return AccountStoreBase<MAP>::UpdateAccounts(transaction, receipt,
                                                   error_code);
    }

    case Transaction::CONTRACT_CREATION: {
      LOG_GENERAL(INFO, "Create contract");

      // bool validToTransferBalance = true;

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      uint64_t createGasPenalty = std::max(
          CONTRACT_CREATE_GAS, (unsigned int)(transaction.GetCode().size() +
                                              transaction.GetData().size()));

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (transaction.GetGasLimit() < createGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimit()
                                          << " less than " << createGasPenalty);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      if (fromAccount->GetBalance() < gasDeposit) {
        LOG_GENERAL(WARNING,
                    "The account doesn't have enough gas to create a contract");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }
      // deduct scilla checker invoke gas
      if (gasRemained < SCILLA_CHECKER_INVOKE_GAS) {
        LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla checker");
        error_code = TxnStatus::INSUFFICIENT_GAS;
        return false;
      } else {
        gasRemained -= SCILLA_CHECKER_INVOKE_GAS;
      }

      // generate address for new contract account
      Address contractAddress =
          Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
      // instantiate the object for contract account
      // ** Remeber to call RemoveAccount if deployment failed halfway
      if (!this->AddAccount(contractAddress, {0, 0})) {
        LOG_GENERAL(WARNING, "AddAccount failed for contract address "
                                 << contractAddress.hex());
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }
      Account* contractAccount = this->GetAccount(contractAddress);
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
      isScilla = !EvmUtils::isEvm(transaction.GetCode());

      bool init = true;
      bool is_library;
      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      uint32_t scilla_version;
      uint32_t evm_version{0};

      try {
        // Initiate the contract account, including setting the contract code
        // store the immutable states
        if (!contractAccount->InitContract(transaction.GetCode(),
                                           transaction.GetData(),
                                           contractAddress, blockNum)) {
          LOG_GENERAL(WARNING, "InitContract failed");
          init = false;
        }

        std::vector<Address> extlibs;
        if (isScilla && !contractAccount->GetContractAuxiliaries(
                            is_library, scilla_version, extlibs)) {
          LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
          this->RemoveAccount(contractAddress);
          error_code = TxnStatus::FAIL_SCILLA_LIB;
          return false;
        }

        if (isScilla && DISABLE_SCILLA_LIB && is_library) {
          LOG_GENERAL(WARNING, "ScillaLib disabled");
          this->RemoveAccount(contractAddress);
          error_code = TxnStatus::FAIL_SCILLA_LIB;
          return false;
        }

        if (isScilla &&
            !PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
          LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
          this->RemoveAccount(contractAddress);
          error_code = TxnStatus::FAIL_SCILLA_LIB;
          return false;
        }

        m_curBlockNum = blockNum;
        if (isScilla && init &&
            !ExportCreateContractFiles(*contractAccount, is_library,
                                       scilla_version, extlibs_exports)) {
          LOG_GENERAL(WARNING, "ExportCreateContractFiles failed");
          init = false;
        }

        if (init && !this->DecreaseBalance(fromAddr, gasDeposit)) {
          init = false;
        }
      } catch (const std::exception& e) {
        LOG_GENERAL(WARNING,
                    "Exception caught in create account (1): " << e.what());
        init = false;
      }

      if (!init) {
        this->RemoveAccount(contractAddress);
        error_code = TxnStatus::FAIL_CONTRACT_INIT;
        return false;
      }

      // prepare IPC with current blockchain info provider.
      auto sbcip = std::make_unique<ScillaBCInfo>(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, contractAddress,
          contractAccount->GetStorageRoot(), scilla_version);
      m_scillaIPCServer->setBCInfoProvider(std::move(sbcip));

      // ************************************************************************
      // Undergo scilla checker
      bool ret_checker = true;
      std::string checkerPrint;

      if (isScilla)
        InvokeInterpreter(CHECKER, checkerPrint, scilla_version, is_library,
                          gasRemained, 0, ret_checker, receipt);
      // 0xabc._version
      // 0xabc._depth.data1
      // 0xabc._type.data1

      std::map<std::string, bytes> t_metadata;
      t_metadata.emplace(
          Contract::ContractStorage::GetContractStorage().GenerateStorageKey(
              contractAddress, SCILLA_VERSION_INDICATOR, {}),
          DataConversion::StringToCharArray(std::to_string(scilla_version)));

      if (isScilla &&
          !ParseContractCheckerOutput(contractAddress, checkerPrint, receipt,
                                      t_metadata, gasRemained, is_library)) {
        ret_checker = false;
      }
      // *************************************************************************
      // Undergo a runner
      bool ret = true;

      if (ret_checker) {
        // deduct scilla runner invoke gas
        if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
          LOG_GENERAL(WARNING, "Not enough gas to invoke the runner");
          receipt.AddError(GAS_NOT_SUFFICIENT);
          ret = false;
        } else {
          gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
        }

        if (ret) {
          std::string runnerPrint;

          if (isScilla) {
            InvokeInterpreter(RUNNER_CREATE, runnerPrint, scilla_version,
                              is_library, gasRemained, transaction.GetAmount(),
                              ret, receipt);
            // parse runner output
            try {
              if (ret && !ParseCreateContract(gasRemained, runnerPrint, receipt,
                                              is_library)) {
                ret = false;
              }
              if (!ret) {
                gasRemained = std::min(
                    transaction.GetGasLimit() - createGasPenalty, gasRemained);
              }
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception caught in create account (2): "
                                       << e.what());
              ret = false;
            }
          } else {
            LOG_GENERAL(INFO, "Invoking EVM with Cumulative Gas "
                                  << gasRemained << " alleged "
                                  << transaction.GetAmount() << " limit "
                                  << transaction.GetGasLimit());

            EvmCallParameters params = {
                contractAddress.hex(),
                fromAddr.hex(),
                DataConversion::CharArrayToString(transaction.GetCode()),
                DataConversion::CharArrayToString(transaction.GetData()),
                gasRemained,
                transaction.GetAmount()};

            // The code leading up to the call to InvokeEVMInterpreter is
            // likely not required.
            std::map<std::string, bytes> t_newmetadata;

            t_newmetadata.emplace(
                Contract::ContractStorage::GenerateStorageKey(
                    contractAddress, CONTRACT_ADDR_INDICATOR, {}),
                contractAddress.asBytes());

            if (!contractAccount->UpdateStates(contractAddress, t_newmetadata,
                                               {}, true)) {
              LOG_GENERAL(WARNING, "Account::UpdateStates failed");
              return false;
            }
            evmproj::CallResponse response;
            gasRemained =
                InvokeEvmInterpreter(contractAccount, RUNNER_CREATE, params,
                                     evm_version, ret, receipt, response);

            ret_checker = true;
          }
        }
      } else {
        gasRemained =
            std::min(transaction.GetGasLimit() - createGasPenalty, gasRemained);
      }
      // *************************************************************************
      // Summary
      boost::multiprecision::uint128_t gasRefund;
      if (!SafeMath<boost::multiprecision::uint128_t>::mul(
              gasRemained, transaction.GetGasPrice(), gasRefund)) {
        this->RemoveAccount(contractAddress);
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }
      if (!this->IncreaseBalance(fromAddr, gasRefund)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for gasRefund");
      }
      if (!ret || !ret_checker) {
        this->m_addressToAccount->erase(contractAddress);

        receipt.SetResult(false);
        if (!ret) {
          receipt.AddError(RUNNER_FAILED);
        }
        if (!ret_checker) {
          receipt.AddError(CHECKER_FAILED);
        }
        receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          this->RemoveAccount(contractAddress);
          error_code = TxnStatus::MATH_ERROR;
          return false;
        }

        if (isScilla)
          LOG_GENERAL(INFO,
                      "Create contract failed, but return true in order to "
                      "change state");

        if (LOG_SC) {
          LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
        }

        if (!isScilla) {
          LOG_GENERAL(INFO,
                      "Executing contract Creation transaction finished "
                      "unsuccessfully");
          return false;
        }
        return true;  // Return true because the states already changed
      }

      if (transaction.GetGasLimit() < gasRemained) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimit()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      /// inserting address to create the uniqueness of the contract merkle
      /// trie
      if (isScilla)
        t_metadata.emplace(Contract::ContractStorage::GenerateStorageKey(
                               contractAddress, CONTRACT_ADDR_INDICATOR, {}),
                           contractAddress.asBytes());

      if (isScilla && !contractAccount->UpdateStates(contractAddress,
                                                     t_metadata, {}, true)) {
        LOG_GENERAL(WARNING, "Account::UpdateStates failed");
        return false;
      }

      /// calculate total gas in receipt
      receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);

      if (isScilla && is_library) {
        m_newLibrariesCreated.emplace_back(contractAddress);
      }
      break;
    }

    case Transaction::CONTRACT_CALL: {
      // reset the storageroot update buffer atomic per transaction
      m_storageRootUpdateBufferAtomic.clear();

      m_originAddr = fromAddr;

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      LOG_GENERAL(INFO, "Call contract");

      uint64_t callGasPenalty = std::max(
          CONTRACT_INVOKE_GAS, (unsigned int)(transaction.GetData().size()));

      if (transaction.GetGasLimit() < callGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimit()
                                          << " less than " << callGasPenalty);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;

        return false;
      }

      if (fromAccount->GetBalance() < gasDeposit + transaction.GetAmount()) {
        LOG_GENERAL(WARNING, "The account (balance: "
                                 << fromAccount->GetBalance()
                                 << ") "
                                    "has not enough balance to deposit the gas "
                                    "price to deposit ("
                                 << gasDeposit
                                 << ") "
                                    "and transfer the amount ("
                                 << transaction.GetAmount()
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

      Account* contractAccount = this->GetAccount(transaction.GetToAddr());
      if (contractAccount == nullptr) {
        LOG_GENERAL(WARNING, "The target contract account doesn't exist");
        error_code = TxnStatus::INVALID_TO_ACCOUNT;
        return false;
      }
      if (contractAccount->GetCode().empty()) {
        LOG_GENERAL(
            WARNING,
            "Trying to call a smart contract that has no code will fail");
        error_code = TxnStatus::NOT_PRESENT;
        return false;
      }
      isScilla = !EvmUtils::isEvm(contractAccount->GetCode());
      bool is_library;
      uint32_t scilla_version;
      uint32_t evm_version{0};

      std::vector<Address> extlibs;
      if (isScilla && !contractAccount->GetContractAuxiliaries(
                          is_library, scilla_version, extlibs)) {
        LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      if (isScilla && is_library) {
        LOG_GENERAL(WARNING, "Library being called");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      if (isScilla && DISABLE_SCILLA_LIB && !extlibs.empty()) {
        LOG_GENERAL(WARNING, "ScillaLib disabled");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      if (isScilla &&
          !PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
        LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
        error_code = TxnStatus::FAIL_SCILLA_LIB;
        return false;
      }

      m_curBlockNum = blockNum;
      if (isScilla &&
          !ExportCallContractFiles(*contractAccount, transaction,
                                   scilla_version, extlibs_exports)) {
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

      m_curGasLimit = transaction.GetGasLimit();
      m_curGasPrice = transaction.GetGasPrice();
      m_curContractAddr = transaction.GetToAddr();
      m_curAmount = transaction.GetAmount();
      m_curNumShards = numShards;

      std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      // prepare IPC with current blockchain info provider.
      auto sbcip = std::make_unique<ScillaBCInfo>(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, m_curContractAddr,
          contractAccount->GetStorageRoot(), scilla_version);
      m_scillaIPCServer->setBCInfoProvider(std::move(sbcip));

      Contract::ContractStorage::GetContractStorage().BufferCurrentState();

      std::string runnerPrint;
      bool ret = true;

      if (isScilla) {
        InvokeInterpreter(
            RUNNER_CALL, runnerPrint, scilla_version, is_library, gasRemained,
            this->GetBalance(transaction.GetToAddr()), ret, receipt);

      } else {
        EvmCallParameters params = {
            m_curContractAddr.hex(),
            fromAddr.hex(),
            DataConversion::CharArrayToString(contractAccount->GetCode()),
            DataConversion::CharArrayToString(transaction.GetData()),
            gasRemained,
            transaction.GetAmount()};

        LOG_GENERAL(WARNING, "contract address is " << params.m_contract
                                                    << " caller account is "
                                                    << params.m_caller);
        evmproj::CallResponse response;
        uint64_t gasUsed =
            InvokeEvmInterpreter(contractAccount, RUNNER_CALL, params,
                                 evm_version, ret, receipt, response);

        if (gasUsed > 0) {
          gasRemained = gasUsed;
        }
      }

      uint32_t tree_depth = 0;

      if (isScilla) {
        if (ret && !ParseCallContract(gasRemained, runnerPrint, receipt,
                                      tree_depth, scilla_version)) {
          receipt.RemoveAllTransitions();
          ret = false;
        }
      }
      if (!ret) {
        Contract::ContractStorage::GetContractStorage().RevertPrevState();
        DiscardAtomics();
        gasRemained =
            std::min(transaction.GetGasLimit() - callGasPenalty, gasRemained);
      } else {
        CommitAtomics();
      }
      boost::multiprecision::uint128_t gasRefund;
      if (!SafeMath<boost::multiprecision::uint128_t>::mul(
              gasRemained, transaction.GetGasPrice(), gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      if (!this->IncreaseBalance(fromAddr, gasRefund)) {
        LOG_GENERAL(WARNING, "IncreaseBalance failed for gasRefund");
      }

      if (transaction.GetGasLimit() < gasRemained) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimit()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
      if (!ret) {
        receipt.SetResult(false);
        receipt.CleanEntry();
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
          return false;
        }

        if (isScilla) {
          LOG_GENERAL(
              INFO,
              "Call contract failed, but return true in order to change state");

          if (LOG_SC) {
            LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
          }

          return true;  // Return true because the states already changed
        } else {
          return false;
        }
      }
      break;
    }
    default: {
      LOG_GENERAL(WARNING, "Txn is not typed correctly")
      error_code = TxnStatus::INCORRECT_TXN_TYPE;
      return false;
    }
    case Transaction::ERROR:
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
  if (LOG_SC) LOG_GENERAL(INFO, "Executing contract transaction finished");

  if (LOG_SC) {
    LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::PopulateExtlibsExports(
    uint32_t scilla_version, const std::vector<Address>& extlibs,
    std::map<Address, std::pair<std::string, std::string>>& extlibs_exports) {
  LOG_MARKER();
  std::function<bool(const std::vector<Address>&,
                     std::map<Address, std::pair<std::string, std::string>>&)>
      extlibsExporter;
  extlibsExporter = [this, &scilla_version, &extlibsExporter](
                        const std::vector<Address>& extlibs,
                        std::map<Address, std::pair<std::string, std::string>>&
                            extlibs_exports) -> bool {
    // export extlibs
    for (const auto& libAddr : extlibs) {
      if (extlibs_exports.find(libAddr) != extlibs_exports.end()) {
        continue;
      }

      Account* libAcc = this->GetAccount(libAddr);
      if (libAcc == nullptr) {
        LOG_GENERAL(WARNING, "libAcc: " << libAddr << " does not exist");
        return false;
      }

      /// Check whether there are caches
      std::string code_path = EXTLIB_FOLDER + '/' + libAddr.hex();
      code_path += LIBRARY_CODE_EXTENSION;
      std::string json_path = EXTLIB_FOLDER + '/' + libAddr.hex() + ".json";
      if (boost::filesystem::exists(code_path) &&
          boost::filesystem::exists(json_path)) {
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

template <class MAP>
bool AccountStoreSC<MAP>::ExportCreateContractFiles(
    const Account& contract, bool is_library, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
  LOG_MARKER();

  boost::filesystem::remove_all("./" + SCILLA_FILES);
  boost::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(boost::filesystem::exists("./" + SCILLA_LOG))) {
    boost::filesystem::create_directories("./" + SCILLA_LOG);
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
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

template <class MAP>
void AccountStoreSC<MAP>::ExportCommonFiles(
    std::ofstream& os, const Account& contract,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
  os.open(INIT_JSON);
  if (LOG_SC) {
    LOG_GENERAL(
        INFO, "init data to export: "
                  << DataConversion::CharArrayToString(contract.GetInitData()));
  }
  os << DataConversion::CharArrayToString(contract.GetInitData());
  os.close();

  for (const auto& extlib_export : extlibs_exports) {
    std::string code_path =
        EXTLIB_FOLDER + '/' + "0x" + extlib_export.first.hex();
    code_path += LIBRARY_CODE_EXTENSION;
    boost::filesystem::remove(code_path);

    os.open(code_path);
    os << extlib_export.second.first;
    os.close();

    std::string init_path =
        EXTLIB_FOLDER + '/' + "0x" + extlib_export.first.hex() + ".json";
    boost::filesystem::remove(init_path);

    os.open(init_path);
    os << extlib_export.second.second;
    os.close();
  }

  // Block Json
  JSONUtils::GetInstance().writeJsontoFile(
      INPUT_BLOCKCHAIN_JSON, ScillaUtils::GetBlockStateJson(m_curBlockNum));
}

template <class MAP>
bool AccountStoreSC<MAP>::ExportContractFiles(
    Account& contract, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
  LOG_MARKER();
  std::chrono::system_clock::time_point tpStart;

  boost::filesystem::remove_all("./" + SCILLA_FILES);
  boost::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(boost::filesystem::exists("./" + SCILLA_LOG))) {
    boost::filesystem::create_directories("./" + SCILLA_LOG);
  }

  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version, m_root_w_version)) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    // Scilla code
    std::ofstream os(INPUT_CODE + CONTRACT_FILE_EXTENSION);
    os << DataConversion::CharArrayToString(contract.GetCode());
    os.close();

    ExportCommonFiles(os, contract, extlibs_exports);

    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      LOG_GENERAL(INFO, "LDB Read (microsec) = " << r_timer_end(tpStart));
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ExportCallContractFiles(
    Account& contract, const Transaction& transaction, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
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
    msgObj["_amount"] = transaction.GetAmount().convert_to<std::string>();

    JSONUtils::GetInstance().writeJsontoFile(INPUT_MESSAGE_JSON, msgObj);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ExportCallContractFiles(
    Account& contract, const Json::Value& contractData, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
  LOG_MARKER();

  if (!ExportContractFiles(contract, scilla_version, extlibs_exports)) {
    LOG_GENERAL(WARNING, "ExportContractFiles failed");
    return false;
  }

  try {
    JSONUtils::GetInstance().writeJsontoFile(INPUT_MESSAGE_JSON, contractData);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseContractCheckerOutput(
    const Address& addr, const std::string& checkerPrint,
    TransactionReceipt& receipt, std::map<std::string, bytes>& metadata,
    uint64_t& gasRemained, bool is_library) {
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

    if (!is_library) {
      if (!root.isMember("contract_info")) {
        receipt.AddError(CHECKER_FAILED);

        if (root.isMember("errors")) {
          receipt.AddException(root["errors"]);
        }

        return false;
      }

      bool hasMap = false;

      auto handleTypeForStateVar = [&](const Json::Value& stateVars) {
        if (!stateVars.isArray()) {
          LOG_GENERAL(WARNING, "An array of state variables expected."
                                   << stateVars.toStyledString());
          return false;
        }
        for (const auto& field : stateVars) {
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
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what() << " checkerPrint: "
                                              << checkerPrint);
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCreateContract(uint64_t& gasRemained,
                                              const std::string& runnerPrint,
                                              TransactionReceipt& receipt,
                                              bool is_library) {
  Json::Value jsonOutput;
  if (!ParseCreateContractOutput(jsonOutput, runnerPrint, receipt)) {
    return false;
  }
  return ParseCreateContractJsonOutput(jsonOutput, gasRemained, receipt,
                                       is_library);
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCreateContractOutput(
    Json::Value& jsonOutput, const std::string& runnerPrint,
    TransactionReceipt& receipt) {
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

  if (runnerPrint.length() == 0) {
    LOG_GENERAL(INFO, "Empty Json string from createContract");
    return false;
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(runnerPrint, jsonOutput)) {
    receipt.AddError(JSON_OUTPUT_CORRUPTED);
    return false;
  }
  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCreateContractJsonOutput(
    const Json::Value& _json, uint64_t& gasRemained,
    TransactionReceipt& receipt, bool is_library) {
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

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContract(uint64_t& gasRemained,
                                            const std::string& runnerPrint,
                                            TransactionReceipt& receipt,
                                            uint32_t tree_depth,
                                            uint32_t scilla_version) {
  Json::Value jsonOutput;
  // This just reparses the JSON, probably due to null values in string
  // screwing up the json
  if (!ParseCallContractOutput(jsonOutput, runnerPrint, receipt)) {
    return false;
  }
  return ParseCallContractJsonOutput(jsonOutput, gasRemained, receipt,
                                     tree_depth, scilla_version);
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContractOutput(
    Json::Value& jsonOutput, const std::string& runnerPrint,
    TransactionReceipt& receipt) {
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

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContractJsonOutput(
    const Json::Value& _json, uint64_t& gasRemained,
    TransactionReceipt& receipt, uint32_t tree_depth,
    uint32_t pre_scilla_version) {
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  // Find remaining gas.
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

  // TODO: ignore messages for EVM
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

  // TODO: ignore _accepted for the EVM.
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

  // TOOD: ignore this.
  if (tree_depth == 0) {
    // first call in a txn
    receipt.AddAccepted(accepted);
  } else {
    if (!receipt.AddAcceptedForLastTransition(accepted)) {
      LOG_GENERAL(WARNING, "AddAcceptedForLastTransition failed");
      return false;
    }
  }

  // TODO: process all the logs for EVM
  Account* contractAccount =
      m_accountStoreAtomic->GetAccount(m_curContractAddr);
  if (contractAccount == nullptr) {
    LOG_GENERAL(WARNING, "contractAccount is null ptr");
    receipt.AddError(CONTRACT_NOT_EXIST);
    return false;
  }
  try {
    for (const auto& e : _json["events"]) {
      LogEntry entry;
      if (!entry.Install(e, m_curContractAddr)) {
        receipt.AddError(LOG_ENTRY_INSTALL_FAILED);
        return false;
      }
      receipt.AddEntry(entry);
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  // TODO: ignore messages
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

  // TODO: Ignore the rest.

  Address recipient;
  Account* account = nullptr;

  if (!ret) {
    // Buffer the Addr for current caller
    Address curContractAddr = m_curContractAddr;
    for (const auto& msg : _json["messages"]) {
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
        AccountStoreBase<MAP>::AddAccount(recipient, {0, 0});
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

      // TODO: ignore this check.
      // check whether the recipient contract is in the same shard with the
      // current contract
      if (!m_curIsDS &&
          (Transaction::GetShardIndex(curContractAddr, m_curNumShards) !=
           Transaction::GetShardIndex(recipient, m_curNumShards))) {
        LOG_GENERAL(WARNING,
                    "another contract doesn't belong to the same shard with "
                    "current contract");
        receipt.AddError(CHAIN_CALL_DIFF_SHARD);
        return false;
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
      auto sbcip = std::make_unique<ScillaBCInfo>(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, recipient,
          account->GetStorageRoot(), scilla_version);
      m_scillaIPCServer->setBCInfoProvider(std::move(sbcip));

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
        LOG_GENERAL(WARNING, "Library being called");
        receipt.AddError(LIBRARY_AS_RECIPIENT);
        return false;
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
      auto sbcip1 = std::make_unique<ScillaBCInfo>(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, recipient,
          account->GetStorageRoot(), scilla_version);
      m_scillaIPCServer->setBCInfoProvider(std::move(sbcip1));

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

template <class MAP>
void AccountStoreSC<MAP>::ProcessStorageRootUpdateBuffer() {
  LOG_MARKER();
  {
    std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
    for (const auto& addr : m_storageRootUpdateBuffer) {
      Account* account = this->GetAccount(addr);
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

template <class MAP>
void AccountStoreSC<MAP>::CleanStorageRootUpdateBuffer() {
  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_storageRootUpdateBuffer.clear();
}

template <class MAP>
bool AccountStoreSC<MAP>::TransferBalanceAtomic(const Address& from,
                                                const Address& to,
                                                const uint128_t& delta) {
  // LOG_MARKER();
  return m_accountStoreAtomic->TransferBalance(from, to, delta);
}

template <class MAP>
void AccountStoreSC<MAP>::CommitAtomics() {
  LOG_MARKER();
  for (const auto& entry : *m_accountStoreAtomic->GetAddressToAccount()) {
    Account* account = this->GetAccount(entry.first);
    if (account != nullptr) {
      *account = entry.second;
    } else {
      // this->m_addressToAccount.emplace(std::make_pair(entry.first,
      // entry.second));
      this->AddAccount(entry.first, entry.second);
    }
  }
}

template <class MAP>
void AccountStoreSC<MAP>::DiscardAtomics() {
  LOG_MARKER();
  m_accountStoreAtomic->Init();
}

template <class MAP>
void AccountStoreSC<MAP>::NotifyTimeout() {
  LOG_MARKER();
  m_txnProcessTimeout = true;
  cv_callContract.notify_all();
}

template <class MAP>
Account* AccountStoreSC<MAP>::GetAccountAtomic(const dev::h160& addr) {
  return m_accountStoreAtomic->GetAccount(addr);
}

template <class MAP>
void AccountStoreSC<MAP>::SetScillaIPCServer(
    std::shared_ptr<ScillaIPCServer> scillaIPCServer) {
  LOG_MARKER();
  m_scillaIPCServer = std::move(scillaIPCServer);
}

template <class MAP>
void AccountStoreSC<MAP>::CleanNewLibrariesCache() {
  for (const auto& addr : m_newLibrariesCreated) {
    boost::filesystem::remove(addr.hex() + LIBRARY_CODE_EXTENSION);
    boost::filesystem::remove(addr.hex() + ".json");
  }
  m_newLibrariesCreated.clear();
}
