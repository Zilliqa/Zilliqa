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

#include "EvmUtils.h"
#include <boost/filesystem.hpp>

#include "Logger.h"
#include "common/Constants.h"

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

Json::Value EvmUtils::GetCallContractJson() {
  Json::Value arr_ret(Json::arrayValue);

  arr_ret.append("0xa6f9959347430609b0ed3fcb27a3e09de9d08ca3");
  arr_ret.append("0xa6f9959347430609b0ed3fcb27a3e09de9d08ca3");
  arr_ret.append("608060405234801561001057600080fd5b50600436106100415760003560e01c80632e64cec11461004657806336b62288146100645780636057361d1461006e575b600080fd5b61004e61008a565b60405161005b91906100d0565b60405180910390f35b61006c610093565b005b6100886004803603810190610083919061011c565b6100ad565b005b60008054905090565b600073ffffffffffffffffffffffffffffffffffffffff16ff5b8060008190555050565b6000819050919050565b6100ca816100b7565b82525050565b60006020820190506100e560008301846100c1565b92915050565b600080fd5b6100f9816100b7565b811461010457600080fd5b50565b600081359050610116816100f0565b92915050565b600060208284031215610132576101316100eb565b5b600061014084828501610107565b9150509291505056fea2646970667358221220c11cc7b07b2f889ced02511e03fe7604a33d010cde91fe1d68869188cf2e3be964736f6c634300080d0033");
  arr_ret.append("36b62288");
  arr_ret.append("00");
  return arr_ret;
}


