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

#include "libPersistence/ContractStorage2.h"
#include "libServer/ScillaIPCServer.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/SafeMath.h"
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
void AccountStoreSC<MAP>::InvokeScillaChecker(std::string& checkerPrint,
                                              bool& ret_checker, int& pid,
                                              const uint64_t& gasRemained,
                                              TransactionReceipt& receipt,
                                              bool is_library) {
  auto func1 = [this, &checkerPrint, &ret_checker, &pid, &gasRemained, &receipt,
                &is_library]() mutable -> void {
    try {
      if (!SysCommand::ExecuteCmd(
              SysCommand::WITH_OUTPUT_PID,
              GetContractCheckerCmdStr(m_root_w_version, is_library,
                                       gasRemained),
              checkerPrint, pid)) {
        LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetContractCheckerCmdStr(
                                 m_root_w_version, is_library, gasRemained));
        receipt.AddError(EXECUTE_CMD_FAILED);
        ret_checker = false;
      }
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Exception caught in SysCommand::ExecuteCmd (1): "
                               << e.what());
      ret_checker = false;
    }

    cv_callContract.notify_all();
  };
  DetachedFunction(1, func1);

  {
    std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
    cv_callContract.wait(lk);
  }
}

template <class MAP>
bool AccountStoreSC<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                         const unsigned int& numShards,
                                         const bool& isDS,
                                         const Transaction& transaction,
                                         TransactionReceipt& receipt) {
  // LOG_MARKER();
  LOG_GENERAL(INFO, "Process txn: " << transaction.GetTranID());
  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);

  m_curIsDS = isDS;
  m_txnProcessTimeout = false;

  const PubKey& senderPubKey = transaction.GetSenderPubKey();
  const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  Address toAddr = transaction.GetToAddr();

  const uint128_t& amount = transaction.GetAmount();

  // Initiate gasRemained
  uint64_t gasRemained = transaction.GetGasLimit();

  // Get the amount of deposit for running this txn
  uint128_t gasDeposit;
  if (!SafeMath<uint128_t>::mul(gasRemained, transaction.GetGasPrice(),
                                gasDeposit)) {
    return false;
  }

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::NON_CONTRACT: {
      // LOG_GENERAL(INFO, "Normal transaction");

      // Disallow normal transaction to contract account
      Account* toAccount = this->GetAccount(toAddr);
      if (toAccount != nullptr) {
        if (toAccount->isContract()) {
          LOG_GENERAL(WARNING, "Contract account won't accept normal txn");
          return false;
        }
      }

      return AccountStoreBase<MAP>::UpdateAccounts(transaction, receipt);
    }
    case Transaction::CONTRACT_CREATION: {
      LOG_GENERAL(INFO, "Create contract");

      bool validToTransferBalance = true;

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        return false;
      }

      uint64_t createGasPenalty = std::max(
          CONTRACT_CREATE_GAS, (unsigned int)(transaction.GetCode().size() +
                                              transaction.GetData().size()));

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (transaction.GetGasLimit() < createGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimit()
                                          << " less than " << createGasPenalty);
        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      if (fromAccount->GetBalance() < gasDeposit) {
        LOG_GENERAL(WARNING,
                    "The account doesn't have enough gas to create a contract");
        return false;
      }
      // Check if the sender has enough balance to pay gasDeposit and transfer
      // amount
      else if (fromAccount->GetBalance() < gasDeposit + amount) {
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
      }

      // deduct scilla checker invoke gas
      if (gasRemained < SCILLA_CHECKER_INVOKE_GAS) {
        LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla checker");
        return false;
      } else {
        gasRemained -= SCILLA_CHECKER_INVOKE_GAS;
      }

      // generate address for new contract account
      toAddr =
          Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
      // instantiate the object for contract account
      if (!this->AddAccount(toAddr, {0, 0})) {
        LOG_GENERAL(WARNING,
                    "AddAccount failed for contract address " << toAddr.hex());
        return false;
      }
      Account* toAccount = this->GetAccount(toAddr);
      if (toAccount == nullptr) {
        LOG_GENERAL(WARNING, "toAccount is null ptr");
        return false;
      }

      bool init = true;

      bool is_library;
      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;

      try {
        // Initiate the contract account, including setting the contract code
        // store the immutable states
        if (!toAccount->InitContract(transaction.GetCode(),
                                     transaction.GetData(), toAddr, blockNum)) {
          LOG_GENERAL(WARNING, "InitContract failed");
          init = false;
        }

        uint32_t scilla_version;
        std::vector<Address> extlibs;
        if (!toAccount->GetContractAuxiliaries(is_library, scilla_version,
                                               extlibs)) {
          LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
          return false;
        }

        if (!PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
          LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
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
      } catch (const std::exception& e) {
        LOG_GENERAL(WARNING,
                    "Exception caught in create account (1): " << e.what());
        init = false;
      }

      if (!init) {
        this->RemoveAccount(toAddr);
        return false;
      }

      // prepare IPC with current contract address
      m_scillaIPCServer->setContractAddress(toAddr);

      // ************************************************************************
      // Undergo scilla checker
      bool ret_checker = true;
      std::string checkerPrint;

      int pid = -1;
      InvokeScillaChecker(checkerPrint, ret_checker, pid, gasRemained, receipt,
                          is_library);

      if (m_txnProcessTimeout) {
        LOG_GENERAL(
            WARNING,
            "Txn processing timeout! Interrupt current contract check, pid: "
                << pid);
        try {
          if (pid >= 0) {
            kill(pid, SIGKILL);
          }
        } catch (const std::exception& e) {
          LOG_GENERAL(WARNING, "Exception caught in kill pid: " << e.what());
        }
        receipt.AddError(EXECUTE_CMD_TIMEOUT);
        ret_checker = false;
      }

      bytes map_depth_data;

      if (ret_checker &&
          !ParseContractCheckerOutput(checkerPrint, receipt, map_depth_data,
                                      gasRemained, is_library)) {
        ret_checker = false;
      }

      if (ret_checker && !is_library) {
        std::map<std::string, bytes> t_map_depth_map;
        t_map_depth_map.emplace(
            Contract::ContractStorage2::GetContractStorage().GenerateStorageKey(
                toAddr, FIELDS_MAP_DEPTH_INDICATOR, {}),
            map_depth_data);
        toAccount->UpdateStates(toAddr, t_map_depth_map, {}, true);
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

          pid = -1;
          auto func2 = [this, &runnerPrint, &ret, &pid, gasRemained, &receipt,
                        &is_library, &extlibs_exports]() mutable -> void {
            try {
              if (!SysCommand::ExecuteCmd(
                      SysCommand::WITH_OUTPUT_PID,
                      GetCreateContractCmdStr(m_root_w_version, is_library,
                                              gasRemained, 0),
                      runnerPrint, pid)) {
                LOG_GENERAL(WARNING,
                            "ExecuteCmd failed: " << GetCreateContractCmdStr(
                                m_root_w_version, is_library, gasRemained, 0));
                receipt.AddError(EXECUTE_CMD_FAILED);
                ret = false;
              }
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING,
                          "Exception caught in SysCommand::ExecuteCmd (2): "
                              << e.what());
              ret = false;
            }

            cv_callContract.notify_all();
          };
          DetachedFunction(1, func2);

          {
            std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
            cv_callContract.wait(lk);
          }

          try {
            if (m_txnProcessTimeout) {
              LOG_GENERAL(WARNING,
                          "Txn processing timeout! Interrupt current contract "
                          "deployment, pid: "
                              << pid);
              if (pid >= 0) {
                kill(pid, SIGKILL);
              }

              receipt.AddError(EXECUTE_CMD_TIMEOUT);
              ret = false;
            }

            if (ret && !ParseCreateContract(gasRemained, runnerPrint, receipt,
                                            is_library)) {
              ret = false;
            }
            if (!ret) {
              gasRemained = std::min(
                  transaction.GetGasLimit() - createGasPenalty, gasRemained);
            }
          } catch (const std::exception& e) {
            LOG_GENERAL(WARNING,
                        "Exception caught in create account (2): " << e.what());
            ret = false;
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
        this->RemoveAccount(toAddr);
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
        receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          this->RemoveAccount(toAddr);
          return false;
        }

        LOG_GENERAL(
            INFO,
            "Create contract failed, but return true in order to change state");

        return true;  // Return true because the states already changed
      }

      if (transaction.GetGasLimit() < gasRemained) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimit()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        return false;
      }

      if (validToTransferBalance) {
        if (!this->TransferBalance(fromAddr, toAddr, amount)) {
          receipt.SetResult(false);
          receipt.AddError(BALANCE_TRANSFER_FAILED);
          receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
          receipt.update();

          if (!this->IncreaseNonce(fromAddr)) {
            this->RemoveAccount(toAddr);
            return false;
          }
          return true;
        }
      }

      toAccount->SetStorageRoot(
          Contract::ContractStorage2::GetContractStorage().GetContractStateHash(
              toAddr, true, true));

      receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);

      if (is_library) {
        m_newLibrariesCreated.emplace_back(toAddr);
      }

      break;
    }
    case Transaction::CONTRACT_CALL: {
      // reset the storageroot update buffer atomic per transaction
      m_storageRootUpdateBufferAtomic.clear();

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        return false;
      }

      LOG_GENERAL(INFO, "Call contract");

      uint64_t callGasPenalty = std::max(
          CONTRACT_INVOKE_GAS, (unsigned int)(transaction.GetData().size()));

      if (transaction.GetGasLimit() < callGasPenalty) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimit()
                                          << " less than " << callGasPenalty);
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
        return false;
      }

      // deduct scilla checker invoke gas
      if (gasRemained < SCILLA_RUNNER_INVOKE_GAS) {
        LOG_GENERAL(WARNING, "Not enough gas to invoke the scilla runner");
        return false;
      } else {
        gasRemained -= SCILLA_RUNNER_INVOKE_GAS;
      }

      m_curSenderAddr = fromAddr;
      m_curEdges = 0;

      Account* toAccount = this->GetAccount(toAddr);
      if (toAccount == nullptr) {
        LOG_GENERAL(WARNING, "The target contract account doesn't exist");
        return false;
      }

      bool is_library;
      uint32_t scilla_version;
      std::vector<Address> extlibs;
      if (!toAccount->GetContractAuxiliaries(is_library, scilla_version,
                                             extlibs)) {
        LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
        return false;
      }

      if (is_library) {
        LOG_GENERAL(WARNING, "Library being called");
        return false;
      }

      std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
      if (!PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
        LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
        return false;
      }

      m_curBlockNum = blockNum;
      if (!ExportCallContractFiles(*toAccount, transaction, scilla_version,
                                   extlibs_exports)) {
        LOG_GENERAL(WARNING, "ExportCallContractFiles failed");
        return false;
      }

      DiscardTransferAtomic();

      if (!this->DecreaseBalance(fromAddr, gasDeposit)) {
        LOG_GENERAL(WARNING, "DecreaseBalance failed");
        return false;
      }

      m_curGasLimit = transaction.GetGasLimit();
      m_curGasPrice = transaction.GetGasPrice();
      m_curContractAddr = toAddr;
      m_curAmount = amount;
      m_curNumShards = numShards;

      std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      // prepare IPC with current contract address
      m_scillaIPCServer->setContractAddress(toAddr);

      std::string runnerPrint;
      bool ret = true;
      int pid = -1;

      auto func = [this, &runnerPrint, &ret, &pid, gasRemained, &receipt,
                   &toAddr, &extlibs_exports]() mutable -> void {
        try {
          if (!SysCommand::ExecuteCmd(
                  SysCommand::WITH_OUTPUT_PID,
                  GetCallContractCmdStr(m_root_w_version, gasRemained,
                                        this->GetBalance(toAddr)),
                  runnerPrint, pid)) {
            LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCallContractCmdStr(
                                     m_root_w_version, gasRemained,
                                     this->GetBalance(toAddr)));
            receipt.AddError(EXECUTE_CMD_FAILED);
            ret = false;
          }
        } catch (const std::exception& e) {
          LOG_GENERAL(WARNING,
                      "Exception caught in call account (1): " << e.what());
          ret = false;
        }
        cv_callContract.notify_all();
      };

      Contract::ContractStorage2::GetContractStorage().BufferCurrentState();
      DetachedFunction(1, func);

      {
        std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
        cv_callContract.wait(lk);
      }

      if (m_txnProcessTimeout) {
        LOG_GENERAL(
            WARNING,
            "Txn processing timeout! Interrupt current contract call, pid: "
                << pid);
        try {
          if (pid >= 0) {
            kill(pid, SIGKILL);
          }
        } catch (const std::exception& e) {
          LOG_GENERAL(WARNING, "Exception caught in kill pid: " << e.what());
        }
        receipt.AddError(EXECUTE_CMD_TIMEOUT);
        ret = false;
      }
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        LOG_GENERAL(DEBUG, "Executed root transition in "
                               << r_timer_end(tpStart) << " microseconds");
      }

      uint32_t tree_depth = 0;

      if (ret && !ParseCallContract(gasRemained, runnerPrint, receipt,
                                    tree_depth, scilla_version)) {
        Contract::ContractStorage2::GetContractStorage().RevertPrevState();
        receipt.RemoveAllTransitions();
        ret = false;
      }
      if (!ret) {
        DiscardTransferAtomic();
        gasRemained =
            std::min(transaction.GetGasLimit() - callGasPenalty, gasRemained);
      } else {
        CommitTransferAtomic();
      }
      boost::multiprecision::uint128_t gasRefund;
      if (!SafeMath<boost::multiprecision::uint128_t>::mul(
              gasRemained, transaction.GetGasPrice(), gasRefund)) {
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
        return false;
      }

      receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
      if (!ret) {
        receipt.SetResult(false);
        receipt.CleanEntry();
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          return false;
        }

        LOG_GENERAL(
            INFO,
            "Call contract failed, but return true in order to change state");

        return true;  // Return true because the states already changed
      }
      break;
    }
    default: {
      LOG_GENERAL(WARNING, "Txn is not typed correctly")
      return false;
    }
  }

  if (!this->IncreaseNonce(fromAddr)) {
    return false;
  }

  receipt.SetResult(true);
  receipt.update();

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::CONTRACT_CALL: {
      /// since txn succeeded, commit the atomic buffer
      m_storageRootUpdateBuffer.insert(m_storageRootUpdateBufferAtomic.begin(),
                                       m_storageRootUpdateBufferAtomic.end());
      LOG_GENERAL(INFO, "Executing contract transaction finished");
      break;
    }
    case Transaction::CONTRACT_CREATION: {
      LOG_GENERAL(INFO, "Executing contract transaction finished");
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

template <class MAP>
Json::Value AccountStoreSC<MAP>::GetBlockStateJson(
    const uint64_t& BlockNum) const {
  Json::Value root;
  Json::Value blockItem;
  blockItem["vname"] = "BLOCKNUMBER";
  blockItem["type"] = "BNum";
  blockItem["value"] = std::to_string(BlockNum);
  root.append(blockItem);

  return root;
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

  if (!PrepareRootPathWVersion(scilla_version)) {
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
  JSONUtils::GetInstance().writeJsontoFile(INPUT_BLOCKCHAIN_JSON,
                                           GetBlockStateJson(m_curBlockNum));
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

  if (!PrepareRootPathWVersion(scilla_version)) {
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
      LOG_GENERAL(DEBUG, "LDB Read (microsec) = " << r_timer_end(tpStart));
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
bool AccountStoreSC<MAP>::PrepareRootPathWVersion(
    const uint32_t& scilla_version) {
  m_root_w_version = SCILLA_ROOT;
  if (ENABLE_SCILLA_MULTI_VERSION) {
    m_root_w_version += '/' + std::to_string(scilla_version);
  }

  if (!boost::filesystem::exists(m_root_w_version)) {
    LOG_GENERAL(WARNING, "Folder for desired version (" << m_root_w_version
                                                        << ") doesn't exists");
    return false;
  }

  return true;
}

template <class MAP>
std::string AccountStoreSC<MAP>::GetContractCheckerCmdStr(
    const std::string& root_w_version, bool is_library,
    const uint64_t& available_gas) {
  std::string cmdStr =
      // "rm -rf " + SCILLA_IPC_SOCKET_PATH + "; " +
      root_w_version + '/' + SCILLA_CHECKER + " -init " + INIT_JSON +
      " -contractinfo -jsonerrors -libdir " + root_w_version + '/' +
      SCILLA_LIB + ":" + EXTLIB_FOLDER + " " + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION) +
      " -gaslimit " + std::to_string(available_gas);

  if (LOG_SC) {
    LOG_GENERAL(INFO, cmdStr);
  }
  return cmdStr;
}

template <class MAP>
std::string AccountStoreSC<MAP>::GetCreateContractCmdStr(
    const std::string& root_w_version, bool is_library,
    const uint64_t& available_gas,
    const boost::multiprecision::uint128_t& balance) {
  std::string cmdStr =
      // "rm -rf " + SCILLA_IPC_SOCKET_PATH + "; " +
      root_w_version + '/' + SCILLA_BINARY + " -init " + INIT_JSON +
      " -ipcaddress " + SCILLA_IPC_SOCKET_PATH + " -iblockchain " +
      INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION) +
      " -gaslimit " + std::to_string(available_gas) + " -jsonerrors -balance " +
      balance.convert_to<std::string>() + " -libdir " + root_w_version + '/' +
      SCILLA_LIB + ":" + EXTLIB_FOLDER;

  if (LOG_SC) {
    LOG_GENERAL(INFO, cmdStr);
  }
  return cmdStr;
}

template <class MAP>
std::string AccountStoreSC<MAP>::GetCallContractCmdStr(
    const std::string& root_w_version, const uint64_t& available_gas,
    const boost::multiprecision::uint128_t& balance) {
  std::string cmdStr =
      // "rm -rf " + SCILLA_IPC_SOCKET_PATH + "; " +
      root_w_version + '/' + SCILLA_BINARY + " -init " + INIT_JSON +
      " -ipcaddress " + SCILLA_IPC_SOCKET_PATH + " -iblockchain " +
      INPUT_BLOCKCHAIN_JSON + " -imessage " + INPUT_MESSAGE_JSON + " -o " +
      OUTPUT_JSON + " -i " + INPUT_CODE + CONTRACT_FILE_EXTENSION +
      " -gaslimit " + std::to_string(available_gas) + " -disable-pp-json" +
      " -disable-validate-json" + " -jsonerrors -balance " +
      balance.convert_to<std::string>() + " -libdir " + root_w_version + '/' +
      SCILLA_LIB + ":" + EXTLIB_FOLDER;

  if (LOG_SC) {
    LOG_GENERAL(INFO, cmdStr);
  }
  return cmdStr;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseContractCheckerOutput(
    const std::string& checkerPrint, TransactionReceipt& receipt,
    bytes& map_depth_data, uint64_t& gasRemained, bool is_library) {
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
        return false;
      }

      Json::Value map_depth_json;
      if (root["contract_info"].isMember("fields")) {
        for (const auto& field : root["contract_info"]["fields"]) {
          if (field.isMember("vname") && field.isMember("depth") &&
              field["depth"].isNumeric()) {
            map_depth_json[field["vname"].asString()] = field["depth"].asInt();
          }
        }
      }

      if (map_depth_json.empty()) {
        map_depth_json = Json::objectValue;
      }

      map_depth_data = DataConversion::StringToCharArray(
          JSONUtils::GetInstance().convertJsontoStr(map_depth_json));
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

  std::ifstream in(OUTPUT_JSON, std::ios::binary);
  std::string outStr;

  if (!in.is_open()) {
    LOG_GENERAL(WARNING,
                "Error opening output file or no output file generated");

    // Check the printout
    if (!runnerPrint.empty()) {
      outStr = runnerPrint;
    } else {
      receipt.AddError(NO_OUTPUT);
      return false;
    }
  } else {
    outStr = {std::istreambuf_iterator<char>(in),
              std::istreambuf_iterator<char>()};
  }

  LOG_GENERAL(
      INFO,
      "Output: " << std::endl
                 << (outStr.length() > MAX_SCILLA_OUTPUT_SIZE_IN_BYTES
                         ? outStr.substr(0, MAX_SCILLA_OUTPUT_SIZE_IN_BYTES) +
                               "\n ... "
                         : outStr));

  if (!JSONUtils::GetInstance().convertStrtoJson(outStr, jsonOutput)) {
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
  // LOG_MARKER();
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }
  std::ifstream in(OUTPUT_JSON, std::ios::binary);
  std::string outStr;

  try {
    if (!in.is_open()) {
      LOG_GENERAL(WARNING,
                  "Error opening output file or no output file generated");

      // Check the printout
      if (!runnerPrint.empty()) {
        outStr = runnerPrint;
      } else {
        receipt.AddError(NO_OUTPUT);
        return false;
      }
    } else {
      outStr = {std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>()};
    }

    LOG_GENERAL(
        INFO,
        "Output: " << std::endl
                   << (outStr.length() > MAX_SCILLA_OUTPUT_SIZE_IN_BYTES
                           ? outStr.substr(0, MAX_SCILLA_OUTPUT_SIZE_IN_BYTES) +
                                 "\n ... "
                           : outStr));

    if (!JSONUtils::GetInstance().convertStrtoJson(outStr, jsonOutput)) {
      receipt.AddError(JSON_OUTPUT_CORRUPTED);
      return false;
    }
    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      LOG_GENERAL(DEBUG, "Parse scilla-runner output (microseconds) = "
                             << r_timer_end(tpStart));
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Exception caught: " << e.what() << " outStr: " << outStr);
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContractJsonOutput(
    const Json::Value& _json, uint64_t& gasRemained,
    TransactionReceipt& receipt, uint32_t tree_depth,
    uint32_t pre_scilla_version) {
  // LOG_MARKER();
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
        LOG_GENERAL(DEBUG,
                    "LDB Write (microseconds) = " << r_timer_end(tpStart));
        LOG_GENERAL(DEBUG, "Gas used = " << (startGas - gasRemained));
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
      input_message["_amount"] = msg["_amount"];
      input_message["_tag"] = msg["_tag"];
      input_message["params"] = msg["params"];

      if (account == nullptr) {
        LOG_GENERAL(WARNING, "account still null");
        receipt.AddError(INTERNAL_ERROR);
        return false;
      }

      bool is_library;
      uint32_t scilla_version;
      std::vector<Address> extlibs;

      if (!account->GetContractAuxiliaries(is_library, scilla_version,
                                           extlibs)) {
        LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
        receipt.AddError(INTERNAL_ERROR);
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

      // prepare IPC with the recipient contract address
      m_scillaIPCServer->setContractAddress(recipient);
      std::string runnerPrint;
      bool result = true;
      int pid = -1;
      auto func = [this, &runnerPrint, &result, &pid, gasRemained, &receipt,
                   &recipient]() mutable -> void {
        try {
          if (!SysCommand::ExecuteCmd(
                  SysCommand::WITH_OUTPUT_PID,
                  GetCallContractCmdStr(m_root_w_version, gasRemained,
                                        this->GetBalance(recipient)),
                  runnerPrint, pid)) {
            LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCallContractCmdStr(
                                     m_root_w_version, gasRemained,
                                     this->GetBalance(recipient)));
            receipt.AddError(EXECUTE_CMD_FAILED);
            result = false;
          }
        } catch (const std::exception& e) {
          LOG_GENERAL(
              WARNING,
              "Exception caught in ParseCallContractJsonOutput: " << e.what());
          result = false;
        }
        cv_callContract.notify_all();
      };

      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      DetachedFunction(1, func);

      {
        std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
        cv_callContract.wait(lk);
      }

      if (m_txnProcessTimeout) {
        LOG_GENERAL(
            WARNING,
            "Txn processing timeout! Interrupt current contract call, pid: "
                << pid);
        try {
          if (pid >= 0) {
            kill(pid, SIGKILL);
          }
        } catch (const std::exception& e) {
          LOG_GENERAL(WARNING,
                      "Exception caught when calling kill pid: " << e.what());
        }
        receipt.AddError(EXECUTE_CMD_TIMEOUT);
        result = false;
      }

      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        LOG_GENERAL(DEBUG, "Executed " << input_message["_tag"] << " in "
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
      account->SetStorageRoot(
          Contract::ContractStorage2::GetContractStorage().GetContractStateHash(
              addr, true, true));
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
void AccountStoreSC<MAP>::CommitTransferAtomic() {
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
void AccountStoreSC<MAP>::DiscardTransferAtomic() {
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