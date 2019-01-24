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

#include "libPersistence/ContractStorage.h"
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
}

template <class MAP>
bool AccountStoreSC<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                         const unsigned int& numShards,
                                         const bool& isDS,
                                         const Transaction& transaction,
                                         TransactionReceipt& receipt) {
  // LOG_MARKER();
  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);

  m_curIsDS = isDS;
  m_txnProcessTimeout = false;

  const PubKey& senderPubKey = transaction.GetSenderPubKey();
  const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  Address toAddr = transaction.GetToAddr();

  const boost::multiprecision::uint128_t& amount = transaction.GetAmount();

  uint64_t gasRemained = transaction.GetGasLimit();

  boost::multiprecision::uint128_t gasDeposit;
  if (!SafeMath<boost::multiprecision::uint128_t>::mul(
          gasRemained, transaction.GetGasPrice(), gasDeposit)) {
    return false;
  }

  if (transaction.GetData().empty() && transaction.GetCode().empty()) {
    // LOG_GENERAL(INFO, "Normal transaction");

    // Disallow normal transaction to contract account
    Account* toAccount = this->GetAccount(toAddr);
    if (toAccount != nullptr) {
      if (toAccount->isContract()) {
        LOG_GENERAL(WARNING,
                    "Contract account won't accept normal transaction");
        return false;
      }
    }

    return AccountStoreBase<MAP>::UpdateAccounts(transaction, receipt);
  }

  bool callContract = false;

  if (transaction.GetData().size() > 0 && toAddr != NullAddress &&
      transaction.GetCode().empty()) {
    callContract = true;
  }

  // Needed by gas handling
  bool validToTransferBalance = true;

  Account* fromAccount = this->GetAccount(fromAddr);
  if (fromAccount == nullptr) {
    LOG_GENERAL(WARNING, "Sender has no balance, reject");
    return false;
  }

  if (transaction.GetCode().size() > 0) {
    if (toAddr != NullAddress) {
      LOG_GENERAL(WARNING, "txn has non-empty code but with valid toAddr");
      return false;
    }

    LOG_GENERAL(INFO, "Create contract");

    if (transaction.GetGasLimit() < CONTRACT_CREATE_GAS) {
      LOG_GENERAL(WARNING,
                  "The gas limit set for this transaction has to be larger than"
                  " the gas to create a contract ("
                      << CONTRACT_CREATE_GAS << ")");
      return false;
    }

    if (fromAccount->GetBalance() < gasDeposit) {
      LOG_GENERAL(WARNING,
                  "The account doesn't have enough gas to create a contract");
      return false;
    } else if (fromAccount->GetBalance() < gasDeposit + amount) {
      LOG_GENERAL(WARNING,
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

    toAddr = Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
    this->AddAccount(toAddr, {0, 0});
    Account* toAccount = this->GetAccount(toAddr);
    if (toAccount == nullptr) {
      LOG_GENERAL(WARNING, "toAccount is null ptr");
      return false;
    }

    bool init = true;
    // Store the immutable states
    if (!toAccount->InitContract(transaction.GetCode(), transaction.GetData(),
                                 toAddr, blockNum, true)) {
      LOG_GENERAL(WARNING, "InitContract failed");
      init = false;
    }

    m_curBlockNum = blockNum;
    if (init && !ExportCreateContractFiles(*toAccount)) {
      LOG_GENERAL(WARNING, "ExportCreateContractFiles failed");
      init = false;
    }

    if (init && !this->DecreaseBalance(fromAddr, gasDeposit)) {
      init = false;
    }

    if (!init) {
      this->RemoveAccount(toAddr);
      return false;
    }

    bool ret = true;
    // Undergo scilla checker
    bool ret_checker = true;
    std::string checkerPrint;
    if (ret) {
      int pid = -1;
      auto func1 = [this, &checkerPrint, &ret_checker, &pid]() mutable -> void {
        if (!SysCommand::ExecuteCmd(SysCommand::WITH_OUTPUT_PID,
                                    GetContractCheckerCmdStr(m_root_w_version),
                                    checkerPrint, pid)) {
          LOG_GENERAL(WARNING,
                      "ExecuteCmd failed: "
                          << GetContractCheckerCmdStr(m_root_w_version));
          ret_checker = false;
        }
        cv_callContract.notify_all();
      };
      DetachedFunction(1, func1);

      {
        std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
        cv_callContract.wait(lk);
      }

      if (m_txnProcessTimeout) {
        LOG_GENERAL(
            WARNING,
            "Txn processing timeout! Interrupt current contract check, pid: "
                << pid);
        if (pid >= 0) {
          kill(-pid, SIGKILL);
        }
        ret_checker = false;
      }
    } else {
      ret_checker = false;
    }

    if (ret && ret_checker && !ParseContractCheckerOutput(checkerPrint)) {
      ret_checker = false;
    }

    // Undergo scilla runner
    std::string runnerPrint;
    if (ret) {
      int pid = -1;
      auto func2 = [this, &runnerPrint, &ret, &pid,
                    gasRemained]() mutable -> void {
        if (!SysCommand::ExecuteCmd(
                SysCommand::WITH_OUTPUT_PID,
                GetCreateContractCmdStr(m_root_w_version, gasRemained),
                runnerPrint, pid)) {
          LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCreateContractCmdStr(
                                   m_root_w_version, gasRemained));
          ret = false;
        }
        cv_callContract.notify_all();
      };
      DetachedFunction(1, func2);

      {
        std::unique_lock<std::mutex> lk(m_MutexCVCallContract);
        cv_callContract.wait(lk);
      }

      if (m_txnProcessTimeout) {
        LOG_GENERAL(WARNING,
                    "Txn processing timeout! Interrupt current contract "
                    "deployment, pid: "
                        << pid);
        if (pid >= 0) {
          kill(-pid, SIGKILL);
        }
        ret = false;
      }
    }

    if (ret && !ParseCreateContract(gasRemained, runnerPrint)) {
      ret = false;
    }
    if (!ret) {
      gasRemained = std::min(transaction.GetGasLimit() - CONTRACT_CREATE_GAS,
                             gasRemained);
    }
    boost::multiprecision::uint128_t gasRefund;
    if (!SafeMath<boost::multiprecision::uint128_t>::mul(
            gasRemained, transaction.GetGasPrice(), gasRefund)) {
      this->m_addressToAccount->erase(toAddr);
      return false;
    }
    this->IncreaseBalance(fromAddr, gasRefund);
    if (!ret || !ret_checker) {
      this->m_addressToAccount->erase(toAddr);

      receipt.SetResult(false);
      if (!ret) {
        receipt.AddError(RUNNER_FAILED);
      }
      if (!ret_checker) {
        receipt.AddError(CHECKER_FAILED);
      }
      receipt.SetCumGas(CONTRACT_CREATE_GAS);
      receipt.update();

      this->IncreaseNonce(fromAddr);

      LOG_GENERAL(
          INFO,
          "Create contract failed, but return true in order to change state");

      return true;  // Return true because the states already changed
    }
  }

  if (!callContract) {
    if (transaction.GetGasLimit() < gasRemained) {
      LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                               << transaction.GetGasLimit() << " gasRemained: "
                               << gasRemained << ". Must be something wrong!");
      return false;
    }

    if (validToTransferBalance) {
      if (!this->TransferBalance(fromAddr, toAddr, amount)) {
        receipt.SetResult(false);
        receipt.AddError(BALANCE_TRANSFER_FAILED);
        receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
        receipt.update();

        this->IncreaseNonce(fromAddr);

        return true;
      }
    }

    receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
  } else {
    LOG_GENERAL(INFO, "Call contract");

    if (transaction.GetGasLimit() < CONTRACT_INVOKE_GAS) {
      LOG_GENERAL(WARNING,
                  "The gas limit set for this transaction has to be larger than"
                  " the minimum gas to invoke contract ("
                      << CONTRACT_INVOKE_GAS << ")");
      return false;
    }

    if (fromAccount->GetBalance() < gasDeposit + amount) {
      LOG_GENERAL(
          WARNING,
          "The account (balance: "
              << fromAccount->GetBalance()
              << ") "
                 "has not enough balance to deposit the gas price to deposit ("
              << gasDeposit
              << ") "
                 "and transfer the amount ("
              << amount
              << ") in the transaction, "
                 "rejected");
      return false;
    }

    m_curSenderAddr = fromAddr;
    m_curDepth = 0;

    Account* toAccount = this->GetAccount(toAddr);
    if (toAccount == nullptr) {
      LOG_GENERAL(WARNING, "The target contract account doesn't exist");
      return false;
    }

    m_curBlockNum = blockNum;
    if (!ExportCallContractFiles(*toAccount, transaction)) {
      LOG_GENERAL(WARNING, "ExportCallContractFiles failed");
      return false;
    }

    DiscardTransferBalanceAtomic();

    if (!this->DecreaseBalance(fromAddr, gasDeposit)) {
      LOG_GENERAL(WARNING, "DecreaseBalance failed");
      return false;
    }

    m_curGasLimit = transaction.GetGasLimit();
    m_curGasPrice = transaction.GetGasPrice();
    m_curContractAddr = toAddr;
    m_curAmount = amount;
    m_curNumShards = numShards;
    m_curTranReceipt.clear();

    std::chrono::system_clock::time_point tpStart;
    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      tpStart = r_timer_start();
    }

    std::string runnerPrint;
    bool ret = true;
    int pid = -1;

    auto func = [this, &runnerPrint, &ret, &pid,
                 gasRemained]() mutable -> void {
      if (!SysCommand::ExecuteCmd(
              SysCommand::WITH_OUTPUT_PID,
              GetCallContractCmdStr(m_root_w_version, gasRemained), runnerPrint,
              pid)) {
        LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCallContractCmdStr(
                                 m_root_w_version, gasRemained));
        ret = false;
        m_curTranReceipt.AddError(EXECUTE_CMD_FAILED);
      }
      cv_callContract.notify_all();
    };

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
      if (pid >= 0) {
        kill(-pid, SIGKILL);
      }
      ret = false;
      m_curTranReceipt.AddError(EXECUTE_CMD_TIMEOUT);
    }
    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      LOG_GENERAL(DEBUG, "Executed root transition in " << r_timer_end(tpStart)
                                                        << " microseconds");
    }

    if (ret && !ParseCallContract(gasRemained, runnerPrint)) {
      ret = false;
    }
    if (!ret) {
      DiscardTransferBalanceAtomic();
      gasRemained = std::min(transaction.GetGasLimit() - CONTRACT_INVOKE_GAS,
                             gasRemained);
    } else {
      CommitTransferBalanceAtomic();
    }
    boost::multiprecision::uint128_t gasRefund;
    if (!SafeMath<boost::multiprecision::uint128_t>::mul(
            gasRemained, transaction.GetGasPrice(), gasRefund)) {
      return false;
    }

    this->IncreaseBalance(fromAddr, gasRefund);
    receipt = m_curTranReceipt;

    if (transaction.GetGasLimit() < gasRemained) {
      LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                               << transaction.GetGasLimit() << " gasRemained: "
                               << gasRemained << ". Must be something wrong!");
      return false;
    }

    receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
    if (!ret) {
      receipt.SetResult(false);
      receipt.update();

      this->IncreaseNonce(fromAddr);

      LOG_GENERAL(
          INFO,
          "Call contract failed, but return true in order to change state");

      return true;  // Return true because the states already changed
    }
  }

  this->IncreaseNonce(fromAddr);

  receipt.SetResult(true);
  receipt.update();

  if (transaction.GetCode().size() > 0 || callContract) {
    LOG_GENERAL(INFO, "Executing contract transaction finished");
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
bool AccountStoreSC<MAP>::ExportCreateContractFiles(const Account& contract) {
  LOG_MARKER();

  boost::filesystem::remove_all("./" + SCILLA_FILES);
  boost::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(boost::filesystem::exists("./" + SCILLA_LOG))) {
    boost::filesystem::create_directories("./" + SCILLA_LOG);
  }

  std::pair<Json::Value, Json::Value> roots;
  uint32_t scilla_version;
  if (!contract.GetStorageJson(roots, true, scilla_version)) {
    LOG_GENERAL(WARNING, "GetStorageJson failed");
    return false;
  }

  if (!PrepareRootPathWVersion(scilla_version)) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  // Scilla code
  std::ofstream os(INPUT_CODE);
  os << DataConversion::CharArrayToString(contract.GetCode());
  os.close();

  // Initialize Json
  JSONUtils::writeJsontoFile(INIT_JSON, roots.first);

  // Block Json
  JSONUtils::writeJsontoFile(INPUT_BLOCKCHAIN_JSON,
                             GetBlockStateJson(m_curBlockNum));

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ExportContractFiles(const Account& contract) {
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

  std::pair<Json::Value, Json::Value> roots;
  uint32_t scilla_version;
  if (!contract.GetStorageJson(roots, true, scilla_version)) {
    LOG_GENERAL(WARNING, "GetStorageJson failed");
    return false;
  }

  if (!PrepareRootPathWVersion(scilla_version)) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  // Scilla code
  std::ofstream os(INPUT_CODE);
  os << DataConversion::CharArrayToString(contract.GetCode());
  os.close();

  // Initialize Json
  JSONUtils::writeJsontoFile(INIT_JSON, roots.first);

  // State Json
  JSONUtils::writeJsontoFile(INPUT_STATE_JSON, roots.second);

  // Block Json
  JSONUtils::writeJsontoFile(INPUT_BLOCKCHAIN_JSON,
                             GetBlockStateJson(m_curBlockNum));

  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(DEBUG, "LDB Read (microsec) = " << r_timer_end(tpStart));
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ExportCallContractFiles(
    const Account& contract, const Transaction& transaction) {
  LOG_MARKER();

  if (!ExportContractFiles(contract)) {
    LOG_GENERAL(WARNING, "ExportContractFiles failed");
    return false;
  }

  // Message Json
  std::string dataStr(transaction.GetData().begin(),
                      transaction.GetData().end());
  Json::Value msgObj;
  if (!JSONUtils::convertStrtoJson(dataStr, msgObj)) {
    return false;
  }
  std::string prepend = "0x";
  msgObj["_sender"] =
      prepend +
      Account::GetAddressFromPublicKey(transaction.GetSenderPubKey()).hex();
  msgObj["_amount"] = transaction.GetAmount().convert_to<std::string>();

  JSONUtils::writeJsontoFile(INPUT_MESSAGE_JSON, msgObj);

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ExportCallContractFiles(
    const Account& contract, const Json::Value& contractData) {
  LOG_MARKER();

  if (!ExportContractFiles(contract)) {
    LOG_GENERAL(WARNING, "ExportContractFiles failed");
    return false;
  }

  JSONUtils::writeJsontoFile(INPUT_MESSAGE_JSON, contractData);

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
    const std::string& root_w_version) {
  std::string cmdStr = root_w_version + '/' + SCILLA_CHECKER + " -libdir " +
                       root_w_version + '/' + SCILLA_LIB + " " + INPUT_CODE;

  LOG_GENERAL(INFO, cmdStr);
  return cmdStr;
}

template <class MAP>
std::string AccountStoreSC<MAP>::GetCreateContractCmdStr(
    const std::string& root_w_version, const uint64_t& available_gas) {
  std::string cmdStr = root_w_version + '/' + SCILLA_BINARY + " -init " +
                       INIT_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON +
                       " -o " + OUTPUT_JSON + " -i " + INPUT_CODE +
                       " -libdir " + root_w_version + '/' + SCILLA_LIB +
                       " -gaslimit " + std::to_string(available_gas);

  LOG_GENERAL(INFO, cmdStr);
  return cmdStr;
}

template <class MAP>
std::string AccountStoreSC<MAP>::GetCallContractCmdStr(
    const std::string& root_w_version, const uint64_t& available_gas) {
  std::string cmdStr =
      root_w_version + '/' + SCILLA_BINARY + " -init " + INIT_JSON +
      " -istate " + INPUT_STATE_JSON + " -iblockchain " +
      INPUT_BLOCKCHAIN_JSON + " -imessage " + INPUT_MESSAGE_JSON + " -o " +
      OUTPUT_JSON + " -i " + INPUT_CODE + " -libdir " + root_w_version + '/' +
      SCILLA_LIB + " -gaslimit " + std::to_string(available_gas) +
      " -disable-pp-json" + " -disable-validate-json";
  LOG_GENERAL(INFO, cmdStr);
  return cmdStr;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseContractCheckerOutput(
    const std::string& checkerPrint) {
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  Json::Value root;
  std::string errors;

  if (!reader->parse(checkerPrint.c_str(),
                     checkerPrint.c_str() + checkerPrint.size(), &root,
                     &errors)) {
    LOG_GENERAL(WARNING, "Failed to parse contract checker output: "
                             << checkerPrint << std::endl
                             << "errors: " << errors);
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCreateContract(uint64_t& gasRemained,
                                              const std::string& runnerPrint) {
  Json::Value jsonOutput;
  if (!ParseCreateContractOutput(jsonOutput, runnerPrint)) {
    return false;
  }
  return ParseCreateContractJsonOutput(jsonOutput, gasRemained);
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCreateContractOutput(
    Json::Value& jsonOutput, const std::string& runnerPrint) {
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

  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errors;

  if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &jsonOutput,
                    &errors)) {
    return true;
  }
  LOG_GENERAL(WARNING, "Failed to parse contract output json: " << errors);
  return false;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCreateContractJsonOutput(
    const Json::Value& _json, uint64_t& gasRemained) {
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
    return false;
  }
  gasRemained = atoi(_json["gas_remaining"].asString().c_str());
  LOG_GENERAL(INFO, "gasRemained: " << gasRemained);

  if (!_json.isMember("message") || !_json.isMember("states") ||
      !_json.isMember("events")) {
    if (_json.isMember("errors")) {
      LOG_GENERAL(WARNING, "Contract creation failed");
    } else {
      LOG_GENERAL(WARNING, "JSON output of this contract is corrupted");
    }
    return false;
  }

  if (_json["message"] == Json::nullValue &&
      _json["states"] == Json::arrayValue &&
      _json["events"] == Json::arrayValue) {
    // LOG_GENERAL(INFO, "Get desired json output from the interpreter for
    // create contract");
    return true;
  }
  LOG_GENERAL(WARNING,
              "Didn't get desired json output from the interpreter for "
              "create contract");
  return false;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContract(uint64_t& gasRemained,
                                            const std::string& runnerPrint) {
  Json::Value jsonOutput;
  if (!ParseCallContractOutput(jsonOutput, runnerPrint)) {
    return false;
  }
  return ParseCallContractJsonOutput(jsonOutput, gasRemained);
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContractOutput(
    Json::Value& jsonOutput, const std::string& runnerPrint) {
  // LOG_MARKER();
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }
  std::ifstream in(OUTPUT_JSON, std::ios::binary);
  std::string outStr;

  if (!in.is_open()) {
    LOG_GENERAL(WARNING,
                "Error opening output file or no output file generated");

    // Check the printout
    if (!runnerPrint.empty()) {
      outStr = runnerPrint;
    } else {
      return false;
    }
  } else {
    outStr = {std::istreambuf_iterator<char>(in),
              std::istreambuf_iterator<char>()};
  }
  LOG_GENERAL(INFO, "Output: " << std::endl << outStr);

  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errors;

  if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &jsonOutput,
                    &errors)) {
    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      LOG_GENERAL(DEBUG, "Parse scilla-runner output (microseconds) = "
                             << r_timer_end(tpStart));
    }
    return true;
  }
  LOG_GENERAL(WARNING, "Failed to parse contract output json: " << errors);
  return false;
}

template <class MAP>
bool AccountStoreSC<MAP>::ParseCallContractJsonOutput(const Json::Value& _json,
                                                      uint64_t& gasRemained) {
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
    m_curTranReceipt.AddError(NO_GAS_REMAINING_FOUND);
    return false;
  }
  uint64_t startGas = gasRemained;
  gasRemained = atoi(_json["gas_remaining"].asString().c_str());
  LOG_GENERAL(INFO, "gasRemained: " << gasRemained);

  if (!_json.isMember("_accepted")) {
    LOG_GENERAL(WARNING,
                "The json output of this contract doesn't contain _accepted");
    m_curTranReceipt.AddError(NO_ACCEPTED_FOUND);
    return false;
  }

  if (!_json.isMember("message") || !_json.isMember("states") ||
      !_json.isMember("events")) {
    if (_json.isMember("errors")) {
      LOG_GENERAL(WARNING, "Call contract failed");
      m_curTranReceipt.AddError(CALL_CONTRACT_FAILED);
    } else {
      LOG_GENERAL(WARNING, "JSON output of this contract is corrupted");
      m_curTranReceipt.AddError(JSON_OUTPUT_CORRUPTED);
    }
    return false;
  }

  if (_json["_accepted"].asString() == "true") {
    // LOG_GENERAL(INFO, "Contract accept amount transfer");
    if (!TransferBalanceAtomic(m_curSenderAddr, m_curContractAddr,
                               m_curAmount)) {
      LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
      m_curTranReceipt.AddError(BALANCE_TRANSFER_FAILED);
      return false;
    }
  } else {
    LOG_GENERAL(WARNING, "Contract refuse amount transfer");
  }

  Account* contractAccount = this->GetAccount(m_curContractAddr);
  if (contractAccount == nullptr) {
    LOG_GENERAL(WARNING, "contractAccount is null ptr");
    m_curTranReceipt.AddError(CONTRACT_NOT_EXIST);
    return false;
  }

  std::vector<Contract::StateEntry> state_entries;
  for (const auto& s : _json["states"]) {
    if (!s.isMember("vname") || !s.isMember("type") || !s.isMember("value")) {
      LOG_GENERAL(WARNING,
                  "Address: " << m_curContractAddr.hex()
                              << ", The json output of states is corrupted");
      m_curTranReceipt.AddError(STATE_CORRUPTED);
      continue;
    }
    std::string vname = s["vname"].asString();
    std::string type = s["type"].asString();
    std::string value = s["value"].isString()
                            ? s["value"].asString()
                            : JSONUtils::convertJsontoStr(s["value"]);

    if (vname != "_balance") {
      state_entries.push_back(std::make_tuple(vname, true, type, value));
    }
  }

  if (!contractAccount->SetStorage(state_entries, true)) {
    LOG_GENERAL(WARNING, "SetStorage failed");
  }

  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(DEBUG, "LDB Write (microseconds) = " << r_timer_end(tpStart));
    LOG_GENERAL(DEBUG, "Gas used = " << (startGas - gasRemained));
  }

  for (const auto& e : _json["events"]) {
    LogEntry entry;
    if (!entry.Install(e, m_curContractAddr)) {
      m_curTranReceipt.AddError(LOG_ENTRY_INSTALL_FAILED);
      return false;
    }
    m_curTranReceipt.AddEntry(entry);
  }

  // If output message is null
  if (_json["message"].isNull()) {
    LOG_GENERAL(INFO,
                "null message in scilla output when invoking a "
                "contract, transaction finished");
    return true;
  }

  // Non-null messages must have few mandatory fields.
  if (!_json["message"].isMember("_tag") ||
      !_json["message"].isMember("_amount") ||
      !_json["message"].isMember("params") ||
      !_json["message"].isMember("_recipient")) {
    LOG_GENERAL(WARNING,
                "The message in the json output of this contract is corrupted");
    m_curTranReceipt.AddError(MESSAGE_CORRUPTED);
    return false;
  }

  Address recipient = Address(_json["message"]["_recipient"].asString());
  if (recipient == Address()) {
    LOG_GENERAL(WARNING, "The recipient can't be null address");
    m_curTranReceipt.AddError(RECEIPT_IS_NULL);
    return false;
  }

  Account* account = this->GetAccount(recipient);

  if (account == nullptr) {
    AccountStoreBase<MAP>::AddAccount(recipient, {0, 0});
  }

  // Recipient is non-contract
  if (!account->isContract()) {
    LOG_GENERAL(INFO, "The recipient is non-contract");
    if (!TransferBalanceAtomic(
            m_curContractAddr, recipient,
            atoi(_json["message"]["_amount"].asString().c_str()))) {
      m_curTranReceipt.AddError(BALANCE_TRANSFER_FAILED);
      return false;
    } else {
      return true;
    }
  }

  // Recipient is contract
  // _tag field is empty
  if (_json["message"]["_tag"].asString().empty()) {
    LOG_GENERAL(INFO,
                "_tag in the scilla output is empty when invoking a "
                "contract, transaction finished");
    return true;
  }

  ++m_curDepth;

  if (m_curDepth > MAX_CONTRACT_DEPTH) {
    LOG_GENERAL(WARNING,
                "maximum contract depth reached, cannot call another contract");
    m_curTranReceipt.AddError(MAX_DEPTH_REACHED);
    return false;
  }

  LOG_GENERAL(INFO, "Call another contract");
  m_curTranReceipt.AddDepth();

  // check whether the recipient contract is in the same shard with the current
  // contract
  if (!m_curIsDS &&
      Transaction::GetShardIndex(m_curContractAddr, m_curNumShards) !=
          Transaction::GetShardIndex(recipient, m_curNumShards)) {
    LOG_GENERAL(WARNING,
                "another contract doesn't belong to the same shard with "
                "current contract");
    m_curTranReceipt.AddError(CHAIN_CALL_DIFF_SHARD);
    return false;
  }

  Json::Value input_message;
  input_message["_sender"] = "0x" + m_curContractAddr.hex();
  input_message["_amount"] = _json["message"]["_amount"];
  input_message["_tag"] = _json["message"]["_tag"];
  input_message["params"] = _json["message"]["params"];

  if (!TransferBalanceAtomic(
          m_curContractAddr, recipient,
          atoi(_json["message"]["_amount"].asString().c_str()))) {
    m_curTranReceipt.AddError(BALANCE_TRANSFER_FAILED);
    return false;
  }

  if (!ExportCallContractFiles(*account, input_message)) {
    LOG_GENERAL(WARNING, "ExportCallContractFiles failed");
    m_curTranReceipt.AddError(PREPARATION_FAILED);
    return false;
  }

  std::string runnerPrint;
  bool result = true;
  int pid = -1;
  auto func = [this, &runnerPrint, &result, &pid,
               gasRemained]() mutable -> void {
    if (!SysCommand::ExecuteCmd(
            SysCommand::WITH_OUTPUT_PID,
            GetCallContractCmdStr(m_root_w_version, gasRemained), runnerPrint,
            pid)) {
      LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCallContractCmdStr(
                               m_root_w_version, gasRemained));
      m_curTranReceipt.AddError(EXECUTE_CMD_FAILED);
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
    LOG_GENERAL(WARNING,
                "Txn processing timeout! Interrupt current contract call, pid: "
                    << pid);
    if (pid >= 0) {
      kill(-pid, SIGKILL);
    }
    m_curTranReceipt.AddError(EXECUTE_CMD_TIMEOUT);
    result = false;
  }

  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(DEBUG, "Executed " << input_message["_tag"] << " in "
                                   << r_timer_end(tpStart) << " microseconds");
  }

  if (!result) {
    return false;
  }

  Address t_address = m_curContractAddr;
  m_curContractAddr = recipient;
  if (!ParseCallContract(gasRemained, runnerPrint)) {
    LOG_GENERAL(WARNING,
                "ParseCallContract failed of calling contract: " << recipient);
    return false;
  }
  this->IncreaseNonce(t_address);
  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::TransferBalanceAtomic(
    const Address& from, const Address& to,
    const boost::multiprecision::uint128_t& delta) {
  // LOG_MARKER();
  return m_accountStoreAtomic->TransferBalance(from, to, delta);
}

template <class MAP>
void AccountStoreSC<MAP>::CommitTransferBalanceAtomic() {
  LOG_MARKER();
  for (const auto& entry : *m_accountStoreAtomic->GetAddressToAccount()) {
    Account* account = this->GetAccount(entry.first);
    if (account != nullptr) {
      account->SetBalance(entry.second.GetBalance());
    } else {
      // this->m_addressToAccount.emplace(std::make_pair(entry.first,
      // entry.second));
      this->AddAccount(entry.first, entry.second);
    }
  }
}

template <class MAP>
void AccountStoreSC<MAP>::DiscardTransferBalanceAtomic() {
  LOG_MARKER();
  m_accountStoreAtomic->Init();
}

template <class MAP>
void AccountStoreSC<MAP>::NotifyTimeout() {
  LOG_MARKER();
  m_txnProcessTimeout = true;
  cv_callContract.notify_all();
}