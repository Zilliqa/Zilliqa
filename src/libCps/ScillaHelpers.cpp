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
#include "libCps/ScillaHelpers.h"

#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/Metrics.h"

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

/* bool ParseCallContract(CpsAccountStoreInterface &acc_store,
                       uint64_t &gasRemained, const std::string &runnerPrint,
                       TransactionReceipt &receipt, uint32_t tree_depth,
                       uint32_t scilla_version);

/// convert the interpreter output into parsable json object for calling
bool ParseCallContractOutput(CpsAccountStoreInterface &acc_store,
                             Json::Value &jsonOutput,
                             const std::string &runnerPrint,
                             TransactionReceipt &receipt);

/// parse the output from interpreter for calling and update states
bool ParseCallContractJsonOutput(CpsAccountStoreInterface &acc_store,
                                 const Json::Value &_json,
                                 uint64_t &gasRemained,
                                 TransactionReceipt &receipt,
                                 uint32_t tree_depth,
                                 uint32_t pre_scilla_version);
*/
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
    tpStart = zil::metrics::r_timer_start();
  }

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version,
                                            acc_store.GetScillaRootVersion())) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    std::string scillaCodeExtension = CONTRACT_FILE_EXTENSION;
    if (acc_store.IsAccountLibrary(contract)) {
      scillaCodeExtension = LIBRARY_CODE_EXTENSION;
    }
    CreateScillaCodeFiles(acc_store, contract, extlibs_exports,
                          scillaCodeExtension);
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(INFO,
                "LDB Read (microsec) = " << zil::metrics::r_timer_end(tpStart));
  }

  return true;
}

bool ScillaHelpers::ExportCallContractFiles(
    CpsAccountStoreInterface &acc_store, const Address &sender,
    const Address &contract, const Transaction &transaction,
    uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();

  if (!ExportContractFiles(acc_store, contract, scilla_version,
                           extlibs_exports)) {
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
    const std::string prepend = "0x";
    msgObj["_sender"] = prepend + sender.hex(),

    msgObj["_origin"] = prepend + sender.hex();
    msgObj["_amount"] = transaction.GetAmountQa().convert_to<std::string>();

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