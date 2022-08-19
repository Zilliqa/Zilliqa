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

#include <vector>
#include "EvmClient.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/EvmUtils.h"

template <class MAP>
void AccountStoreSC<MAP>::EvmCallRunner(
    INVOKE_TYPE invoke_type, EvmCallParameters& params, const uint32_t& version,
    bool& ret, TransactionReceipt& receipt,
    evmproj::CallResponse& evmReturnValues) {
  auto call_already_finished{false};

  auto worker = [this, &params, &invoke_type, &ret, &version,
                 &call_already_finished, &evmReturnValues]() mutable -> void {
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
    std::chrono::duration<int, std::milli> ks(30000);  // 30 seconds
    std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
    if (!call_already_finished) {
      if (LOG_SC) {
        LOG_GENERAL(WARNING, "Waiting on lock");
      }
      auto tv = cv_callContract.wait_for(lk, ks);
      if (tv == std::cv_status::no_timeout) {
        if (LOG_SC) {
          LOG_GENERAL(WARNING, "lock released normally");
        }
      } else {
        if (LOG_SC) {
          LOG_GENERAL(WARNING, "lock released due to timeout");
        }
        m_txnProcessTimeout = true;
      }
    } else {
      LOG_GENERAL(INFO, "Call functions already finished!");
    }
  }
  if (m_txnProcessTimeout) {
    if (LOG_SC) {
      LOG_GENERAL(WARNING, "Txn processing timeout!");
    }
    EvmClient::GetInstance().CheckClient(0, true);
    if (LOG_SC) {
      LOG_GENERAL(WARNING, "Txn Checked Client returned!");
    }
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    ret = false;
  }
}

template <class MAP>
uint64_t AccountStoreSC<MAP>::InvokeEvmInterpreter(
    Account* contractAccount, INVOKE_TYPE invoke_type,
    EvmCallParameters& params, const uint32_t& version, bool& ret,
    TransactionReceipt& receipt, evmproj::CallResponse& evmReturnValues) {
  EvmCallRunner(invoke_type, params, version, ret, receipt, evmReturnValues);

  if (not evmReturnValues.GetSuccess()) {
    LOG_GENERAL(WARNING, evmReturnValues.ExitReason());
  }

  // switch ret to reflect our overall success
  ret = evmReturnValues.GetSuccess() ? ret : false;

  if (!evmReturnValues.Logs().empty()) {
    Json::Value _json = Json::arrayValue;

    for (const auto& logJsonString : evmReturnValues.Logs()) {
      LOG_GENERAL(INFO, "Evm return value logs: " << logJsonString);

      try {
        Json::Value tmp;
        Json::Reader _reader;
        if (_reader.parse(logJsonString, tmp)) {
          _json.append(tmp);
        } else {
          LOG_GENERAL(WARNING, "Parsing json unsuccessful " << logJsonString);
        }
      } catch (std::exception& e) {
        LOG_GENERAL(WARNING, "Exception: " << e.what());
      }
    }
    receipt.AddJsonEntry(_json);
  }

  auto gas = evmReturnValues.Gas();

  std::map<std::string, bytes> states;
  std::vector<std::string> toDeletes;
  // parse the return values from the call to evm.
  for (const auto& it : evmReturnValues.m_apply) {
    if (it->OperationType() == "delete") {
      // be careful with this call needs further testing.
      // TODO: likely needs fixing, test case: remove an account and then revert
      // a transaction. this will likely remove the account anyways, despite the
      // revert.
      this->RemoveAccount(Address(it->Address()));
    } else {
      // Get the account that this apply instruction applies to
      Account* targetAccount = this->GetAccountAtomic(Address(it->Address()));
      if (targetAccount == nullptr) {
        if (!this->AddAccountAtomic(Address(it->Address()), {0, 0})) {
          LOG_GENERAL(WARNING, "AddAccount failed for address "
                                   << Address(it->Address()).hex());
          continue;
        }
        targetAccount = this->GetAccountAtomic(Address(it->Address()));
        if (targetAccount == nullptr) {
          LOG_GENERAL(WARNING, "failed to retrieve new account for address "
                                   << Address(it->Address()).hex());
          continue;
        }
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
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to reset storage " << e.what());
        }

        // If Instructed to reset the Code do so and call SetImmutable to reset
        // the hash
        try {
          if (it->hasCode() && it->Code().size() > 0) {
            targetAccount->SetImmutable(
                DataConversion::StringToCharArray("EVM" + it->Code()), {});
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
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
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to update state on the contract "
                          << e.what());
        }

        try {
          if (it->hasBalance() && it->Balance().size()) {
            targetAccount->SetBalance(uint128_t(it->Balance()));
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(
              WARNING,
              "Exception thrown trying to update balance on target Account "
                  << e.what());
        }

        try {
          if (it->hasNonce() && it->Nonce().size()) {
            targetAccount->SetNonce(std::stoull(it->Nonce(), nullptr, 0));
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to set Nonce on target Account "
                          << e.what());
        }
        // Mark the Address as updated
        m_storageRootUpdateBufferAtomic.emplace(it->Address());
      }
    }
  }

  // send code to be executed
  if (invoke_type == RUNNER_CREATE) {
    contractAccount->SetImmutable(DataConversion::StringToCharArray(
                                      "EVM" + evmReturnValues.ReturnedBytes()),
                                  contractAccount->GetInitData());
  }
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
    LOG_GENERAL(INFO, "Called Evm, response:" << response);
  }

  return ret;
}

template <class MAP>
bool AccountStoreSC<MAP>::UpdateAccountsEvm(const uint64_t& blockNum,
                                            const unsigned int& numShards,
                                            const bool& isDS,
                                            const Transaction& transaction,
                                            TransactionReceipt& receipt,
                                            TxnStatus& error_code) {
  LOG_MARKER();

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Process txn: " << transaction.GetTranID());
  }

  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_curIsDS = isDS;
  m_txnProcessTimeout = false;
  bool isScilla{true};

  error_code = TxnStatus::NOT_PRESENT;

  const Address fromAddr = transaction.GetSenderAddr();

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

      isScilla = !EvmUtils::isEvm(transaction.GetCode());

      uint64_t createGasPenalty =
          isScilla ? std::max(CONTRACT_CREATE_GAS,
                              (unsigned int)(transaction.GetCode().size() +
                                             transaction.GetData().size()))
                   : 32000 / 100;  //  100 is the current gas scaling factor.
                                   //  TODO: move to config.

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (transaction.GetGasLimit() < createGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimit()
                                          << " less than " << createGasPenalty);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      if (fromAccount->GetBalance() < gasDeposit + transaction.GetAmount()) {
        LOG_GENERAL(WARNING,
                    "The account doesn't have enough gas to create a contract");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }

      if (isScilla) {
        // deduct scilla checker invoke gas
        if (gasRemained < SCILLA_CHECKER_INVOKE_GAS) {
          LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla checker");
          error_code = TxnStatus::INSUFFICIENT_GAS;
          return false;
        } else {
          gasRemained -= SCILLA_CHECKER_INVOKE_GAS;
        }
      }

      // generate address for new contract account
      Address contractAddress =
          Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
      // instantiate the object for contract account
      // ** Remeber to call RemoveAccount if deployment failed halfway
      Account* contractAccount;
      if (isScilla) {
        if (!this->AddAccount(contractAddress, {0, 0})) {
          LOG_GENERAL(WARNING, "AddAccount failed for contract address "
                                   << contractAddress.hex());
          error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
          return false;
        }
        contractAccount = this->GetAccount(contractAddress);
        if (contractAccount == nullptr) {
          LOG_GENERAL(WARNING, "contractAccount is null ptr");
          error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
          return false;
        }
      } else {
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
      }
      if (transaction.GetCode().empty()) {
        LOG_GENERAL(WARNING,
                    "Creating a contract with empty code is not feasible.");
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }

      bool init = true;
      bool is_library;
      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      uint32_t scilla_version;
      uint32_t evm_version{0};

      try {
        // Initiate the contract account, including setting the contract code
        // store the immutable states
        if (isScilla) {  // For the EVM we don't need to before we make a call.
          if (!contractAccount->InitContract(transaction.GetCode(),
                                             transaction.GetData(),
                                             contractAddress, blockNum)) {
            LOG_GENERAL(WARNING, "InitContract failed");
            init = false;
          }
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
        if (isScilla) {
          this->RemoveAccount(contractAddress);
        }
        error_code = TxnStatus::FAIL_CONTRACT_INIT;
        return false;
      }

      // prepare IPC with current blockchain info provider.
      auto sbcip = std::make_unique<ScillaBCInfo>(
          m_curBlockNum, m_curDSBlockNum, m_originAddr, contractAddress,
          contractAccount->GetStorageRoot(), scilla_version);

      if (m_scillaIPCServer) {
        m_scillaIPCServer->setBCInfoProvider(std::move(sbcip));
      } else {
        LOG_GENERAL(
            WARNING,
            "Scilla IPC server is not setup correctly - detected null object");
      }

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
        if (isScilla) {
          // deduct scilla runner invoke gas
          if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
            LOG_GENERAL(WARNING, "Not enough gas to invoke the runner");
            receipt.AddError(GAS_NOT_SUFFICIENT);
            ret = false;
          } else {
            gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
          }
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

            if (!TransferBalanceAtomic(fromAddr, contractAddress,
                                       transaction.GetAmount())) {
              error_code = TxnStatus::INSUFFICIENT_BALANCE;
              LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
              return false;
            }

            EvmCallParameters params = {
                contractAddress.hex(),
                fromAddr.hex(),
                DataConversion::CharArrayToString(transaction.GetCode()),
                DataConversion::CharArrayToString(transaction.GetData()),
                transaction.GetGasLimit(),
                transaction.GetAmount()};

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
        if (isScilla) {
          this->RemoveAccount(contractAddress);
        }
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }
      if (!this->IncreaseBalance(fromAddr, gasRefund)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for gasRefund");
      }
      if (!ret || !ret_checker) {
        if (!isScilla) {
          DiscardAtomics();
        } else {
          this->m_addressToAccount->erase(contractAddress);
        }

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
          if (isScilla) {
            this->RemoveAccount(contractAddress);
          }
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
      } else {
        if (!isScilla) {
          CommitAtomics();
        }
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

      Account* contractAccount = this->GetAccount(transaction.GetToAddr());
      if (contractAccount == nullptr) {
        LOG_GENERAL(WARNING, "The target contract account doesn't exist");
        error_code = TxnStatus::INVALID_TO_ACCOUNT;
        return false;
      }
      isScilla = !EvmUtils::isEvm(contractAccount->GetCode());

      LOG_GENERAL(INFO, "Call contract");

      uint64_t callGasPenalty = 0;
      if (isScilla) {
        callGasPenalty = std::max(CONTRACT_INVOKE_GAS,
                                  (unsigned int)(transaction.GetData().size()));

        if (transaction.GetGasLimit() < callGasPenalty) {
          LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimit()
                                            << " less than " << callGasPenalty);
          error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;

          return false;
        }
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

      if (isScilla) {
        // deduct scilla checker invoke gas
        if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
          LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla runner");
          error_code = TxnStatus::INSUFFICIENT_GAS;
          return false;
        } else {
          gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
        }
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
      if (m_scillaIPCServer) {
        m_scillaIPCServer->setBCInfoProvider(std::move(sbcip));
      } else {
        LOG_GENERAL(WARNING, "m_scillaIPCServer not Initialised");
      }

      Contract::ContractStorage::GetContractStorage().BufferCurrentState();

      std::string runnerPrint;
      bool ret = true;

      if (isScilla) {
        InvokeInterpreter(
            RUNNER_CALL, runnerPrint, scilla_version, is_library, gasRemained,
            this->GetBalance(transaction.GetToAddr()), ret, receipt);

      } else {
        if (!TransferBalanceAtomic(fromAddr, m_curContractAddr,
                                   transaction.GetAmount())) {
          error_code = TxnStatus::INSUFFICIENT_BALANCE;
          LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
          return false;
        }

        EvmCallParameters params = {
            m_curContractAddr.hex(),
            fromAddr.hex(),
            DataConversion::CharArrayToString(contractAccount->GetCode()),
            DataConversion::CharArrayToString(transaction.GetData()),
            transaction.GetGasLimit(),
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

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Executing contract transaction finished");
    LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
  }

  return true;
}
