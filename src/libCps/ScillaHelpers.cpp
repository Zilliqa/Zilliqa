/*
 * Copyright (C) 2023 Zilliqa
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

#include "common/Constants.h"

#include "libData/AccountData/TransactionReceipt.h"

#include "libScilla/ScillaUtils.h"

#include "libCps/CpsAccountStoreInterface.h"
#include "libCps/CpsRunScilla.h"
#include "libCps/ScillaHelpers.h"

#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

#include <boost/filesystem.hpp>
#include <fstream>

namespace libCps {

constexpr auto MAX_SCILLA_OUTPUT_SIZE_IN_BYTES = 5120;

bool ScillaHelpers::ParseCreateContract(uint64_t &gasRemained,
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

bool ScillaHelpers::ParseCreateContractOutput(Json::Value &jsonOutput,
                                              const std::string &runnerPrint,
                                              TransactionReceipt &receipt) {
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

bool ScillaHelpers::ParseCreateContractJsonOutput(const Json::Value &_json,
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

ScillaCallParseResult ScillaHelpers::ParseCallContract(
    CpsAccountStoreInterface &acc_store, ScillaArgs &scillaArgs,
    const std::string &runnerPrint, TransactionReceipt &receipt,
    uint32_t scillaVersion) {
  Json::Value jsonOutput;
  auto parseResult =
      ParseCallContractOutput(acc_store, jsonOutput, runnerPrint, receipt);
  if (!parseResult.success) {
    return parseResult;
  }
  return ParseCallContractJsonOutput(acc_store, scillaArgs, jsonOutput, receipt,
                                     scillaVersion);
}

/// convert the interpreter output into parsable json object for calling
ScillaCallParseResult ScillaHelpers::ParseCallContractOutput(
    CpsAccountStoreInterface &acc_store, Json::Value &jsonOutput,
    const std::string &runnerPrint, TransactionReceipt &receipt) {
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
    return {};
  }
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(INFO, "Parse scilla-runner output (microseconds) = "
                          << r_timer_end(tpStart));
  }

  return {true, false, {}};
}

/// parse the output from interpreter for calling and update states
ScillaCallParseResult ScillaHelpers::ParseCallContractJsonOutput(
    CpsAccountStoreInterface &acc_store, ScillaArgs &scillaArgs,
    const Json::Value &_json, TransactionReceipt &receipt,
    uint32_t preScillaVersion) {
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (!_json.isMember("gas_remaining")) {
    LOG_GENERAL(
        WARNING,
        "The json output of this contract didn't contain gas_remaining");
    if (scillaArgs.gasLimit > CONTRACT_INVOKE_GAS) {
      scillaArgs.gasLimit -= CONTRACT_INVOKE_GAS;
    } else {
      scillaArgs.gasLimit = 0;
    }
    receipt.AddError(NO_GAS_REMAINING_FOUND);
    return {};
  }
  // uint64_t startGas = gasRemained;
  try {
    scillaArgs.gasLimit = std::min(
        scillaArgs.gasLimit,
        boost::lexical_cast<uint64_t>(_json["gas_remaining"].asString()));
  } catch (...) {
    LOG_GENERAL(WARNING, "_amount " << _json["gas_remaining"].asString()
                                    << " is not numeric");
    return {};
  }
  LOG_GENERAL(INFO, "gasRemained: " << scillaArgs.gasLimit);

  if (!_json.isMember("messages") || !_json.isMember("events")) {
    if (_json.isMember("errors")) {
      LOG_GENERAL(WARNING, "Call contract failed");
      receipt.AddError(CALL_CONTRACT_FAILED);
      receipt.AddException(_json["errors"]);
    } else {
      LOG_GENERAL(WARNING, "JSON output of this contract is corrupted");
      receipt.AddError(OUTPUT_ILLEGAL);
    }
    return {};
  }

  if (!_json.isMember("_accepted")) {
    LOG_GENERAL(WARNING,
                "The json output of this contract doesn't contain _accepted");
    receipt.AddError(NO_ACCEPTED_FOUND);
    return {};
  }

  ScillaCallParseResult results;

  results.success = true;
  results.accepted = (_json["_accepted"].asString() == "true");

  if (scillaArgs.depth == 0) {
    // first call in a txn
    receipt.AddAccepted(results.accepted);
  } else {
    if (!receipt.AddAcceptedForLastTransition(results.accepted)) {
      LOG_GENERAL(WARNING, "AddAcceptedForLastTransition failed");
      return {};
    }
  }

  try {
    for (const auto &e : _json["events"]) {
      LogEntry entry;
      if (!entry.Install(e, scillaArgs.dest)) {
        receipt.AddError(LOG_ENTRY_INSTALL_FAILED);
        return {};
      }
      receipt.AddLogEntry(entry);
    }
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return {};
  }

  if (_json["messages"].type() != Json::arrayValue) {
    LOG_GENERAL(INFO, "messages is not in array value");
    return {};
  }

  // If output message is null
  if (_json["messages"].empty()) {
    LOG_GENERAL(INFO,
                "empty message in scilla output when invoking a "
                "contract, transaction finished");
  }

  for (const auto &msg : _json["messages"]) {
    // Non-null messages must have few mandatory fields.
    if (!msg.isMember("_tag") || !msg.isMember("_amount") ||
        !msg.isMember("params") || !msg.isMember("_recipient")) {
      LOG_GENERAL(
          WARNING,
          "The message in the json output of this contract is corrupted");
      receipt.AddError(MESSAGE_CORRUPTED);
      return {};
    }

    Amount amount;
    try {
      amount = Amount::fromQa(
          boost::lexical_cast<uint128_t>(msg["_amount"].asString()));
    } catch (...) {
      LOG_GENERAL(WARNING,
                  "_amount " << msg["_amount"].asString() << " is not numeric");
      return {};
    }

    const auto recipient = Address(msg["_recipient"].asString());

    LOG_GENERAL(WARNING, "NEW message: from: " << scillaArgs.dest
                                               << ", to: " << recipient);

    /*if (IsNullAddress(recipient)) {
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
  */
    // Recipient is contract
    // _tag field is empty
    const bool isNextContract = !msg["_tag"].asString().empty();

    // Stop going further as transaction has been finished
    if (!isNextContract) {
      results.entries.emplace_back(ScillaCallParseResult::SingleResult{
          {}, recipient, amount, isNextContract});
      continue;
    }

    receipt.AddTransition(scillaArgs.dest, msg, scillaArgs.depth);
    receipt.AddEdge();
    ++scillaArgs.edge;

    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      LOG_GENERAL(INFO, "LDB Write (microseconds) = " << r_timer_end(tpStart));
    }

    if (scillaArgs.edge > MAX_CONTRACT_EDGES) {
      LOG_GENERAL(
          WARNING,
          "maximum contract edges reached, cannot call another contract");
      receipt.AddError(MAX_EDGES_REACHED);
      return {};
    }

    bool isLibrary;
    std::vector<Address> extlibs;
    uint32_t scillaVersion;

    if (!acc_store.GetContractAuxiliaries(recipient, isLibrary, scillaVersion,
                                          extlibs)) {
      LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
      receipt.AddError(INTERNAL_ERROR);
      return {};
    }
    if (scillaVersion != preScillaVersion) {
      LOG_GENERAL(WARNING, "Scilla version inconsistent");
      receipt.AddError(VERSION_INCONSISTENT);
      return {};
    }

    Json::Value inputMessage;
    inputMessage["_sender"] = "0x" + scillaArgs.dest.hex();
    inputMessage["_origin"] = "0x" + scillaArgs.origin.hex();
    inputMessage["_amount"] = msg["_amount"];
    inputMessage["_tag"] = msg["_tag"];
    inputMessage["params"] = msg["params"];

    results.entries.emplace_back(ScillaCallParseResult::SingleResult{
        std::move(inputMessage), recipient, amount, isNextContract});

    /*
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

    if (!ExportCallContractFiles(*account, inputMessage, scilla_version,
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
      LOG_GENERAL(INFO, "Executed " << inputMessage["_tag"] << " in "
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
  } */
  }

  return results;
}

void ScillaHelpers::ExportCommonFiles(
    CpsAccountStoreInterface &acc_store, std::ofstream &os,
    const Address &contract,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  os.open(INIT_JSON);
  if (LOG_SC) {
    LOG_GENERAL(INFO,
                "init data to export: " << DataConversion::CharArrayToString(
                    acc_store.GetContractInitData(contract)));
  }
  os << DataConversion::CharArrayToString(
      acc_store.GetContractInitData(contract));
  os.close();

  for (const auto &extlib_export : extlibs_exports) {
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
}

bool ScillaHelpers::ExportCreateContractFiles(
    CpsAccountStoreInterface &acc_store, const Address &address,
    bool is_library, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();

  boost::filesystem::remove_all("./" + SCILLA_FILES);
  boost::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(boost::filesystem::exists("./" + SCILLA_LOG))) {
    boost::filesystem::create_directories("./" + SCILLA_LOG);
  }

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version,
                                            acc_store.GetScillaRootVersion())) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    // Scilla code
    std::ofstream os(INPUT_CODE + (is_library ? LIBRARY_CODE_EXTENSION
                                              : CONTRACT_FILE_EXTENSION));
    os << DataConversion::CharArrayToString(acc_store.GetContractCode(address));
    os.close();

    ExportCommonFiles(acc_store, os, address, extlibs_exports);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

bool ScillaHelpers::ExportContractFiles(
    CpsAccountStoreInterface &acc_store, const Address &contract,
    uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
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

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version,
                                            acc_store.GetScillaRootVersion())) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    std::string scillaCodeExtension = CONTRACT_FILE_EXTENSION;
    if (acc_store.IsAccountALibrary(contract)) {
      scillaCodeExtension = LIBRARY_CODE_EXTENSION;
    }
    CreateScillaCodeFiles(acc_store, contract, extlibs_exports,
                          scillaCodeExtension);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(INFO, "LDB Read (microsec) = " << r_timer_end(tpStart));
  }

  return true;
}

bool ScillaHelpers::ExportCallContractFiles(
    CpsAccountStoreInterface &acc_store, const Address &sender,
    const Address &contract, const zbytes &data, const Amount &amount,
    uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();
  LOG_GENERAL(WARNING, "ExportCallContractFiles:, contract: "
                           << contract.hex() << ", sender: " << sender.hex()
                           << ", origin: " << sender.hex());
  if (!ExportContractFiles(acc_store, contract, scilla_version,
                           extlibs_exports)) {
    LOG_GENERAL(WARNING, "ExportContractFiles failed");
    return false;
  }

  try {
    // Message Json
    std::string dataStr(data.begin(), data.end());
    Json::Value msgObj;
    if (!JSONUtils::GetInstance().convertStrtoJson(dataStr, msgObj)) {
      return false;
    }
    const std::string prepend = "0x";
    msgObj["_sender"] = prepend + sender.hex(),

    msgObj["_origin"] = prepend + sender.hex();
    msgObj["_amount"] = amount.toQa().convert_to<std::string>();

    JSONUtils::GetInstance().writeJsontoFile(INPUT_MESSAGE_JSON, msgObj);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

bool ScillaHelpers::ExportCallContractFiles(
    CpsAccountStoreInterface &acc_store, const Address &contract,
    const Json::Value &contractData, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();
  LOG_GENERAL(WARNING, "ExportCallContractFiles: contract: " << contract.hex());
  if (!ExportContractFiles(acc_store, contract, scilla_version,
                           extlibs_exports)) {
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

void ScillaHelpers::CreateScillaCodeFiles(
    CpsAccountStoreInterface &acc_store, const Address &contract,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports,
    const std::string &scillaCodeExtension) {
  LOG_MARKER();
  // Scilla code
  std::ofstream os(INPUT_CODE + scillaCodeExtension);
  os << DataConversion::CharArrayToString(acc_store.GetContractCode(contract));
  os.close();

  ExportCommonFiles(acc_store, os, contract, extlibs_exports);
}

bool ScillaHelpers::ParseContractCheckerOutput(
    CpsAccountStoreInterface &acc_store, const Address &addr,
    const std::string &checkerPrint, TransactionReceipt &receipt,
    std::map<std::string, zbytes> &metadata, uint64_t &gasRemained,
    bool is_library) {
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

      /*auto handleTypeForStateVar = [addr = std::cref(addr)](
                                       const Json::Value &stateVars,
                                       std::map<std::string, zbytes> &metadata,
                                       CpsAccountStoreInterface &acc_store,
                                       bool &hasMap) {
        if (!stateVars.isArray()) {
          LOG_GENERAL(WARNING, "An array of state variables expected."
                                   << stateVars.toStyledString());
          return false;
        }
        for (const auto &field : stateVars) {
          if (field.isMember("vname") && field.isMember("depth") &&
              field["depth"].isNumeric() && field.isMember("type")) {
            metadata.emplace(
                acc_store.GenerateContractStorageKey(
                    addr, MAP_DEPTH_INDICATOR, {field["vname"].asString()}),
                DataConversion::StringToCharArray(field["depth"].asString()));
            if (!hasMap && field["depth"].asInt() > 0) {
              hasMap = true;
            }
            metadata.emplace(
                acc_store.GenerateContractStorageKey(
                    addr, TYPE_INDICATOR, {field["vname"].asString()}),
                DataConversion::StringToCharArray(field["type"].asString()));
          } else {
            LOG_GENERAL(WARNING,
                        "Unexpected field detected" << field.toStyledString());
            return false;
          }
        }
        return true;
      };*/
      if (root["contract_info"].isMember("fields")) {
        const Json::Value &fields = root["contract_info"]["fields"];
        ///----------------------
        if (!fields.isArray()) {
          LOG_GENERAL(WARNING, "An array of state variables expected."
                                   << fields.toStyledString());
          return false;
        }
        for (const auto &field : fields) {
          if (field.isMember("vname") && field.isMember("depth") &&
              field["depth"].isNumeric() && field.isMember("type")) {
            metadata.emplace(
                acc_store.GenerateContractStorageKey(
                    addr, MAP_DEPTH_INDICATOR, {field["vname"].asString()}),
                DataConversion::StringToCharArray(field["depth"].asString()));
            if (!hasMap && field["depth"].asInt() > 0) {
              hasMap = true;
            }
            metadata.emplace(
                acc_store.GenerateContractStorageKey(
                    addr, TYPE_INDICATOR, {field["vname"].asString()}),
                DataConversion::StringToCharArray(field["type"].asString()));
          } else {
            LOG_GENERAL(WARNING,
                        "Unexpected field detected" << field.toStyledString());
            return false;
          }
        }
        /// ----------------------
        // if (!handleTypeForStateVar(root["contract_info"]["fields"], metadata,
        // acc_store, hasMap)) { return false;
        // }
      }
    }
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what() << " checkerPrint: "
                                              << checkerPrint);
    return false;
  }

  return true;
}

bool ScillaHelpers::PopulateExtlibsExports(
    CpsAccountStoreInterface &acc_store, uint32_t scilla_version,
    const std::vector<Address> &extlibs,
    std::map<Address, std::pair<std::string, std::string>> &extlibs_exports) {
  LOG_MARKER();

  std::function<bool(const std::vector<Address> &,
                     std::map<Address, std::pair<std::string, std::string>> &)>
      extlibsExporter;
  extlibsExporter = [&acc_store, &scilla_version, &extlibsExporter](
                        const std::vector<Address> &extlibs,
                        std::map<Address, std::pair<std::string, std::string>>
                            &extlibs_exports) -> bool {
    // export extlibs
    for (const auto &libAddr : extlibs) {
      if (extlibs_exports.find(libAddr) != extlibs_exports.end()) {
        continue;
      }

      if (!acc_store.AccountExistsAtomic(libAddr)) {
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

      if (!acc_store.GetContractAuxiliaries(libAddr, ext_is_lib,
                                            ext_scilla_version, ext_extlibs)) {
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
          DataConversion::CharArrayToString(acc_store.GetContractCode(libAddr)),
          DataConversion::CharArrayToString(
              acc_store.GetContractInitData(libAddr))};

      if (!extlibsExporter(ext_extlibs, extlibs_exports)) {
        return false;
      }
    }

    return true;
  };
  return extlibsExporter(extlibs, extlibs_exports);
}

}  // namespace libCps