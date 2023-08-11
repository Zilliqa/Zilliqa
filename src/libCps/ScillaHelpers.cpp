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
#include "libCps/CpsContext.h"
#include "libCps/CpsRunScilla.h"
#include "libCps/ScillaHelpers.h"

#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

#include <fstream>

namespace libCps {

constexpr auto MAX_SCILLA_OUTPUT_SIZE_IN_BYTES = 5120;

void ScillaHelpers::ExportCommonFiles(
    const std::vector<uint8_t> &contract_init_data, std::ofstream &os,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  return ScillaUtils::ExportCommonFiles(contract_init_data, os,
                                        extlibs_exports);
}

bool ScillaHelpers::ExportCreateContractFiles(
    const std::vector<uint8_t> &contract_code,
    const std::vector<uint8_t> &contract_init_data, bool is_library,
    std::string &scilla_root_version, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>
        &extlibs_exports) {
  LOG_MARKER();

  return ScillaUtils::ExportCreateContractFiles(
      contract_code, contract_init_data, is_library, scilla_root_version,
      scilla_version, extlibs_exports);
}

bool ScillaHelpers::ExportContractFiles(
    CpsAccountStoreInterface &acc_store, const Address &contract,
    uint32_t scilla_version,
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

  ExportCommonFiles(acc_store.GetContractInitData(contract), os,
                    extlibs_exports);
}

bool ScillaHelpers::ParseContractCheckerOutput(
    CpsAccountStoreInterface &acc_store, const Address &addr,
    const std::string &checkerPrint, TransactionReceipt &receipt,
    std::map<std::string, zbytes> &metadata, GasTracker &gasTracker,
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
      if (gasTracker.GetCoreGas() > CONTRACT_CREATE_GAS) {
        gasTracker.DecreaseByCore(CONTRACT_CREATE_GAS);
      } else {
        gasTracker.DecreaseByCore(gasTracker.GetCoreGas());
      }
      receipt.AddError(NO_GAS_REMAINING_FOUND);
      return false;
    }
    uint64_t gasRemained = 0;
    try {
      gasRemained = std::min(
          gasTracker.GetCoreGas(),
          boost::lexical_cast<uint64_t>(root["gas_remaining"].asString()));
    } catch (...) {
      LOG_GENERAL(WARNING, "_amount " << root["gas_remaining"].asString()
                                      << " is not numeric");
      return false;
    }

    gasTracker.SetGasCore(gasRemained);
    LOG_GENERAL(INFO, "gasRemained: " << gasTracker.GetCoreGas());

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
      if (std::filesystem::exists(code_path) &&
          std::filesystem::exists(json_path)) {
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