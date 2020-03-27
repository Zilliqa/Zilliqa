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

string ScillaUtils::GetContractCheckerCmdStr(const string& root_w_version,
                                             bool is_library,
                                             const uint64_t& available_gas) {
  string cmdStr =
      // "rm -rf " + SCILLA_IPC_SOCKET_PATH + "; " +
      root_w_version + '/' + SCILLA_CHECKER + " -init " + INIT_JSON +
      " -contractinfo -jsonerrors -libdir " + root_w_version + '/' +
      SCILLA_LIB + ":" + EXTLIB_FOLDER + " " + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION) +
      " -gaslimit " + to_string(available_gas);

  if (LOG_SC) {
    LOG_GENERAL(INFO, cmdStr);
  }
  return cmdStr;
}

Json::Value ScillaUtils::GetContractCheckerJson(const string& root_w_version,
                                                bool is_library,
                                                const uint64_t& available_gas) {
  Json::Value ret;
  ret["argv"]
      .append("-init")
      .append(INIT_JSON)
      .append("-libdir")
      .append(root_w_version + '/' + SCILLA_LIB + ":" + EXTLIB_FOLDER)
      .append(INPUT_CODE +
              (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION))
      .append("-gaslimit")
      .append(to_string(available_gas))
      .append("-contractinfo")
      .append("-jsonerrors");
  return ret;
}

string ScillaUtils::GetCreateContractCmdStr(const string& root_w_version,
                                            bool is_library,
                                            const uint64_t& available_gas,
                                            const uint128_t& balance) {
  string cmdStr =
      // "rm -rf " + SCILLA_IPC_SOCKET_PATH + "; " +
      root_w_version + '/' + SCILLA_BINARY + " -init " + INIT_JSON +
      " -ipcaddress " + SCILLA_IPC_SOCKET_PATH + " -iblockchain " +
      INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE +
      (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION) +
      " -gaslimit " + to_string(available_gas) + " -jsonerrors -balance " +
      balance.convert_to<string>() + " -libdir " + root_w_version + '/' +
      SCILLA_LIB + ":" + EXTLIB_FOLDER;

  if (LOG_SC) {
    LOG_GENERAL(INFO, cmdStr);
  }
  return cmdStr;
}

Json::Value ScillaUtils::GetCreateContractJson(const string& root_w_version,
                                               bool is_library,
                                               const uint64_t& available_gas,
                                               const uint128_t& balance) {
  Json::Value ret;
  ret["argv"]
      .append("-init")
      .append(INIT_JSON)
      .append("-ipcaddress")
      .append(SCILLA_IPC_SOCKET_PATH)
      .append("iblockchain")
      .append(INPUT_BLOCKCHAIN_JSON)
      .append("-o")
      .append(OUTPUT_JSON)
      .append("-i")
      .append(INPUT_CODE +
              (is_library ? LIBRARY_CODE_EXTENSION : CONTRACT_FILE_EXTENSION))
      .append("-gaslimit")
      .append(to_string(available_gas))
      .append("-balance")
      .append(balance.convert_to<string>())
      .append("-libdir")
      .append(root_w_version + '/' + SCILLA_LIB + ":" + EXTLIB_FOLDER)
      .append("-jsonerrors");

  return ret;
}

string ScillaUtils::GetCallContractCmdStr(const string& root_w_version,
                                          const uint64_t& available_gas,
                                          const uint128_t& balance) {
  string cmdStr =
      // "rm -rf " + SCILLA_IPC_SOCKET_PATH + "; " +
      root_w_version + '/' + SCILLA_BINARY + " -init " + INIT_JSON +
      " -ipcaddress " + SCILLA_IPC_SOCKET_PATH + " -iblockchain " +
      INPUT_BLOCKCHAIN_JSON + " -imessage " + INPUT_MESSAGE_JSON + " -o " +
      OUTPUT_JSON + " -i " + INPUT_CODE + CONTRACT_FILE_EXTENSION +
      " -gaslimit " + to_string(available_gas) + " -disable-pp-json" +
      " -disable-validate-json" + " -jsonerrors -balance " +
      balance.convert_to<string>() + " -libdir " + root_w_version + '/' +
      SCILLA_LIB + ":" + EXTLIB_FOLDER;

  if (LOG_SC) {
    LOG_GENERAL(INFO, cmdStr);
  }
  return cmdStr;
}

Json::Value ScillaUtils::GetCallContractJson(const string& root_w_version,
                                             const uint64_t& available_gas,
                                             const uint128_t& balance) {
  Json::Value ret;
  ret["argv"]
      .append("-init")
      .append(INIT_JSON)
      .append("-ipcaddress")
      .append(SCILLA_IPC_SOCKET_PATH)
      .append("-iblockchain")
      .append(INPUT_BLOCKCHAIN_JSON)
      .append("-imessage")
      .append(INPUT_MESSAGE_JSON)
      .append("-o")
      .append(OUTPUT_JSON)
      .append("-i")
      .append(INPUT_CODE + CONTRACT_FILE_EXTENSION)
      .append("-gaslimit")
      .append(to_string(available_gas))
      .append("-balance")
      .append(balance.convert_to<string>())
      .append("-libdir")
      .append(root_w_version + '/' + SCILLA_LIB + ":" + EXTLIB_FOLDER)
      .append("-disable-validate-json")
      .append("-jsonerrors");

  return ret;
}