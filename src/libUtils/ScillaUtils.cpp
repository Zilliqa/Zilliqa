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

#include <boost/filesystem.hpp>

#include "Logger.h"
#include "common/Constants.h"

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

Json::Value ScillaUtils::GetBlockStateJson(const uint64_t& BlockNum) {
  Json::Value root;
  Json::Value blockItem;
  blockItem["vname"] = "BLOCKNUMBER";
  blockItem["type"] = "BNum";
  blockItem["value"] = to_string(BlockNum);
  root.append(blockItem);

  return root;
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
  ret["argv"].append("-iblockchain");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INPUT_BLOCKCHAIN_JSON);
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
                                             const uint128_t& balance) {
  Json::Value ret;
  ret["argv"].append("-init");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INIT_JSON);
  ret["argv"].append("-ipcaddress");
  ret["argv"].append(SCILLA_IPC_SOCKET_PATH);
  ret["argv"].append("-iblockchain");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INPUT_BLOCKCHAIN_JSON);
  ret["argv"].append("-imessage");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INPUT_MESSAGE_JSON);
  ret["argv"].append("-o");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     OUTPUT_JSON);
  ret["argv"].append("-i");
  ret["argv"].append(boost::filesystem::current_path().string() + '/' +
                     INPUT_CODE + CONTRACT_FILE_EXTENSION);
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
