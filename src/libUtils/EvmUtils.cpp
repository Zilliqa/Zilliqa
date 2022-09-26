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

#include <array>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include "EvmUtils.h"

#include "JsonUtils.h"
#include "Logger.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/GasConv.h"

using namespace std;
using namespace boost::multiprecision;

Json::Value EvmUtils::GetEvmCallJson(const EvmCallParameters& params) {
  Json::Value arr_ret(Json::arrayValue);

  arr_ret.append(params.m_contract);
  arr_ret.append(params.m_caller);
  std::string code;
  try {
    // take off the EVM prefix
    if ((not params.m_code.empty()) && params.m_code.size() >= 3 &&
        params.m_code[0] == 'E' && params.m_code[1] == 'V' &&
        params.m_code[2] == 'M') {
      std::copy(params.m_code.begin() + 3, params.m_code.end(),
                std::back_inserter(code));
      arr_ret.append(code);
    } else {
      LOG_GENERAL(WARNING,
                  "Sending to EVM-DS code without a standard prefix,"
                  " is this intended ? re-evaluate this warning"
                      << arr_ret);
      arr_ret.append(params.m_code);
    }
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING,
                "Exception caught attempting to slice off prefix of "
                "code"
                " is this intended ? re-evaluate this warning"
                    << arr_ret);
    LOG_GENERAL(WARNING, "Sending a blank code array for continuation purposes"
                             << arr_ret);
    arr_ret.append("");
  }
  arr_ret.append(params.m_data);
  arr_ret.append(params.m_apparent_value.str());
  arr_ret.append(Json::Value::UInt64(params.m_available_gas));

  Json::Value extras;
  extras["chain_id"] = ETH_CHAINID;
  extras["block_timestamp"] = params.m_extras.block_timestamp;
  extras["block_gas_limit"] = params.m_extras.block_gas_limit;
  extras["block_difficulty"] = params.m_extras.block_difficulty;
  extras["block_number"] = params.m_extras.block_number;
  extras["gas_price"] = params.m_extras.gas_price;

  return arr_ret;
}

bool EvmUtils::isEvm(const bytes& code) {
  if (not ENABLE_EVM) {
    return false;
  }

  if (code.empty()) {
    // returning false which means it will behave as if it was a scilla only
    // Scilla handles scilla smartContracts and non contracts
    return false;
  }

  if (code.size() < 4) {
    return false;
  }

  auto const hasEvm = (code[0] == 'E' && code[1] == 'V' && code[2] == 'M');

  return hasEvm;
}

bool GetEvmCallExtras(const uint64_t& blockNum, EvmCallExtras& extras) {
  TxBlockSharedPtr txBlockSharedPtr;
  if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum, txBlockSharedPtr)) {
    LOG_GENERAL(WARNING, "Could not get blockNum tx block " << blockNum);
    return false;
  }

  DSBlockSharedPtr dsBlockSharedPtr;
  uint64_t dsBlockNum = txBlockSharedPtr->GetHeader().GetDSBlockNum();
  if (!BlockStorage::GetBlockStorage().GetDSBlock(dsBlockNum,
                                                  dsBlockSharedPtr)) {
    LOG_GENERAL(WARNING, "Could not get blockNum DS block " << dsBlockNum);
    return false;
  }

  std::stringstream gasPrice;
  gasPrice << dsBlockSharedPtr->GetHeader().GetGasPrice();
  extras.block_timestamp =
      txBlockSharedPtr->GetTimestamp() / 1000000;  // microseconds to seconds
  extras.block_gas_limit =
      DS_MICROBLOCK_GAS_LIMIT * GasConv::GetScalingFactor();
  extras.block_difficulty = dsBlockSharedPtr->GetHeader().GetDifficulty();
  extras.block_number = blockNum;
  extras.gas_price = gasPrice.str();
  return true;
}
