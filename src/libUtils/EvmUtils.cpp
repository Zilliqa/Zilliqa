/*
* Copyright (C) 2022 Zilliqa
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
#include <string>
#include <mutex>

#include "EvmUtils.h"
#include <boost/filesystem.hpp>

#include "Logger.h"
#include "JsonUtils.h"
#include "common/Constants.h"
#include "libUtils/RunnerDetails.h"

using namespace std;
using namespace boost::multiprecision;

bool EvmUtils::PrepareRootPathWVersion(const uint32_t& evm_version,
                                          string& root_w_version) {
  root_w_version = EVM_ROOT;
  if (ENABLE_EVM_MULTI_VERSION) {
    root_w_version += '/' + to_string(evm_version);
  }

  if (!boost::filesystem::exists(root_w_version)) {
    LOG_GENERAL(WARNING, "Folder for desired version (" << root_w_version
                                                        << ") doesn't exists");
    return false;
  }

  return true;
}


std::string
EvmUtils::GetDataFromItemData(const std::string& itemData){
  Json::Value  root;
  Json::Reader reader;
  std::string  reply;
  try {
    if (reader.parse(itemData, root)) {
      std::string testString = root[0]["vname"].asString();
      if ( testString != "_evm_version"){
        LOG_GENERAL(WARNING, "Init Parameter does not appear to be formatted correctly " << testString);
      }
      reply = root[1]["data"].asString();
    }
  }  catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Exception caught: " << e.what() << " itemData: " << itemData);
  }
  return reply;
}

Json::Value
EvmUtils::GetCreateContractJson(const RunnerDetails& details) {
  Json::Value arr_ret(Json::arrayValue);


  arr_ret.append(details.m_from);
  arr_ret.append(details.m_to);
  // The next two parameters come directly from the user in the code and init struct
  //
  arr_ret.append(details.m_code);
  arr_ret.append(GetDataFromItemData(details.m_data));
  arr_ret.append("00");

  return arr_ret;
}

Json::Value
EvmUtils::GetCallContractJson(const RunnerDetails& details) {
  Json::Value arr_ret(Json::arrayValue);

  arr_ret.append(details.m_from);
  arr_ret.append(details.m_to);
  // code and data here may be different as they are calling the contract with
  // values returned from the EVM.
  arr_ret.append(details.m_code);
  arr_ret.append(GetDataFromItemData(details.m_data));
  arr_ret.append("00");

  return arr_ret;
}

