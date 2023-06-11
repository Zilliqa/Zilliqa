/*
 * Copyright (C) 2020 Zilliqa
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

#include "ScillaUtils.h"

#include <boost/filesystem/operations.hpp>

#include "common/Constants.h"
#include "libData/AccountStore/AccountStore.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

bool ScillaUtils::PrepareRootPathWVersion(const uint32_t& scilla_version,
                                          string& root_w_version) {
  root_w_version = SCILLA_ROOT;
  if (ENABLE_SCILLA_MULTI_VERSION) {
    root_w_version += '/' + to_string(scilla_version);
  }

  if (!boost::filesystem::exists(root_w_version)) {
    LOG_GENERAL(WARNING, "Folder for desired version (" << root_w_version
                                                        << ") doesn't exists");
    return false;
  }

  return true;
}

Json::Value ScillaUtils::GetContractCheckerJson(const string& root_w_version,
                                                bool is_library,
                                                const uint64_t& available_gas) {
  Json::Value ret;
  ret["argv"].append("-init");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INIT_JSON);
  ret["argv"].append("-libdir");
  ret["argv"].append(root_w_version + '/' + SCILLA_LIB + ":" +
                     boost::filesystem::current_path().string() + '/' +
                     EXTLIB_FOLDER);
  ret["argv"].append(
      boost::filesystem::current_path().string() + '/' + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION));
  ret["argv"].append("-gaslimit");
  ret["argv"].append(to_string(available_gas));
  ret["argv"].append("-contractinfo");
  ret["argv"].append("-jsonerrors");
  return ret;
}

Json::Value ScillaUtils::GetCreateContractJson(const string& root_w_version,
                                               bool is_library,
                                               const uint64_t& available_gas,
                                               const uint128_t& balance) {
  Json::Value ret;
  ret["argv"].append("-init");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INIT_JSON);
  ret["argv"].append("-ipcaddress");
  ret["argv"].append(SCILLA_IPC_SOCKET_PATH);
  ret["argv"].append("-o");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     OUTPUT_JSON);
  ret["argv"].append("-i");
  ret["argv"].append(
      boost::filesystem::current_path().string() + '/' + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION));
  ret["argv"].append("-gaslimit");
  ret["argv"].append(to_string(available_gas));
  ret["argv"].append("-balance");
  ret["argv"].append(balance.convert_to<string>());
  ret["argv"].append("-libdir");
  ret["argv"].append(root_w_version + '/' + SCILLA_LIB + ":" +
                     boost::filesystem::current_path().string() + '/' +
                     EXTLIB_FOLDER);
  ret["argv"].append("-jsonerrors");

  return ret;
}

Json::Value ScillaUtils::GetCallContractJson(const string& root_w_version,
                                             const uint64_t& available_gas,
                                             const uint128_t& balance,
                                             const bool& is_library) {
  Json::Value ret;
  ret["argv"].append("-init");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INIT_JSON);
  ret["argv"].append("-ipcaddress");
  ret["argv"].append(SCILLA_IPC_SOCKET_PATH);
  ret["argv"].append("-imessage");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INPUT_MESSAGE_JSON);
  ret["argv"].append("-o");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     OUTPUT_JSON);
  ret["argv"].append("-i");
  ret["argv"].append(
      boost::filesystem::current_path().string() + '/' + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION));
  ret["argv"].append("-gaslimit");
  ret["argv"].append(to_string(available_gas));
  ret["argv"].append("-balance");
  ret["argv"].append(balance.convert_to<string>());
  ret["argv"].append("-libdir");
  ret["argv"].append(root_w_version + '/' + SCILLA_LIB + ":" +
                     boost::filesystem::current_path().string() + '/' +
                     EXTLIB_FOLDER);
  ret["argv"].append("-jsonerrors");
  ret["argv"].append("-pplit");
  ret["argv"].append(SCILLA_PPLIT_FLAG ? "true" : "false");

  return ret;
}

Json::Value ScillaUtils::GetDisambiguateJson() {
  Json::Value ret;
  ret["argv"].append("-iinit");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INIT_JSON);
  ret["argv"].append("-ipcaddress");
  ret["argv"].append(SCILLA_IPC_SOCKET_PATH);
  ret["argv"].append("-oinit");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     OUTPUT_JSON);
  ret["argv"].append("-i");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INPUT_CODE + CONTRACT_FILE_EXTENSION);

  return ret;
}

void ScillaUtils::ExportCommonFiles(
    const std::vector<uint8_t>& contract_init_data, std::ofstream& os,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
  os.open(INIT_JSON);
  if (LOG_SC) {
    LOG_GENERAL(INFO,
                "init data to export: "
                    << DataConversion::CharArrayToString(contract_init_data));
  }
  os << DataConversion::CharArrayToString(contract_init_data);
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
}

bool ScillaUtils::ExportCreateContractFiles(
    const std::vector<uint8_t>& contract_code,
    const std::vector<uint8_t>& contract_init_data, bool is_library,
    std::string& scilla_root_version, uint32_t scilla_version,
    const std::map<Address, std::pair<std::string, std::string>>&
        extlibs_exports) {
  LOG_MARKER();

  boost::filesystem::remove_all("./" + SCILLA_FILES);
  boost::filesystem::create_directories("./" + SCILLA_FILES);

  if (!(boost::filesystem::exists("./" + SCILLA_LOG))) {
    boost::filesystem::create_directories("./" + SCILLA_LOG);
  }

  if (!ScillaUtils::PrepareRootPathWVersion(scilla_version,
                                            scilla_root_version)) {
    LOG_GENERAL(WARNING, "PrepareRootPathWVersion failed");
    return false;
  }

  try {
    // Scilla code
    std::ofstream os(INPUT_CODE + (is_library ? LIBRARY_CODE_EXTENSION
                                              : CONTRACT_FILE_EXTENSION));
    os << DataConversion::CharArrayToString(contract_code);
    os.close();

    ScillaUtils::ExportCommonFiles(contract_init_data, os, extlibs_exports);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  return true;
}

bool ScillaUtils::PopulateExtlibsExports(
    AccountStore& acc_store, uint32_t scilla_version,
    const std::vector<Address>& extlibs,
    std::map<Address, std::pair<std::string, std::string>>& extlibs_exports) {
  LOG_MARKER();

  std::function<bool(const std::vector<Address>&,
                     std::map<Address, std::pair<std::string, std::string>>&)>
      extlibsExporter;
  extlibsExporter = [&acc_store, &scilla_version, &extlibsExporter](
                        const std::vector<Address>& extlibs,
                        std::map<Address, std::pair<std::string, std::string>>&
                            extlibs_exports) -> bool {
    // export extlibs
    for (const auto& libAddr : extlibs) {
      if (extlibs_exports.find(libAddr) != extlibs_exports.end()) {
        continue;
      }

      if (!acc_store.IsAccountExist(libAddr)) {
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
      auto* account = acc_store.GetAccount(libAddr);
      if (account == nullptr) {
        return false;
      }
      if (!account->GetContractAuxiliaries(ext_is_lib, ext_scilla_version,
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
          DataConversion::CharArrayToString(account->GetCode()),
          DataConversion::CharArrayToString(account->GetInitData())};

      if (!extlibsExporter(ext_extlibs, extlibs_exports)) {
        return false;
      }
    }

    return true;
  };
  return extlibsExporter(extlibs, extlibs_exports);
}
