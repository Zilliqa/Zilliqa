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

#include <iostream>
#include <mutex>
#include <string>
#include <array>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include "EvmUtils.h"

#include "JsonUtils.h"
#include "Logger.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"

using namespace std;
using namespace boost::multiprecision;

bool EvmUtils::PrepareRootPathWVersion(string& root_w_version) {
  root_w_version = EVM_ROOT;

  if (!boost::filesystem::exists(root_w_version)) {
    LOG_GENERAL(WARNING, "Folder for desired version (" << root_w_version
                                                        << ") doesn't exists");
    return false;
  }

  return true;
}


Json::Value EvmUtils::GetCreateContractJson(EvmCallParameters& params) {
  Json::Value arr_ret(Json::arrayValue);

  arr_ret.append(params.m_owner);
  arr_ret.append(params.m_contract);
  std::vector<unsigned char> code;
  try {
    // take off the EVM prefix
    std::copy(params.m_code.begin() + 3, params.m_code.end(),
              std::back_inserter(code));
    arr_ret.append(params.m_code);
  } catch(std::exception &e){
    arr_ret.append(params.m_code);
  }
  arr_ret.append(params.m_data);
  arr_ret.append(params.m_balance.str());
  arr_ret.append(Json::Value::UInt64(params.m_available_gas));

  return arr_ret;
}

Json::Value EvmUtils::GetCallContractJson(const EvmCallParameters& params) {
  Json::Value arr_ret(Json::arrayValue);

  arr_ret.append(params.m_owner);
  arr_ret.append(params.m_contract);
  std::vector<unsigned char> code;
  try {
    // take off the EVM prefix
    std::copy(params.m_code.begin() + 3, params.m_code.end(),
              std::back_inserter(code));
    arr_ret.append(params.m_code);
  } catch(std::exception &e){
    arr_ret.append(params.m_code);
  }
  arr_ret.append(params.m_data);
  arr_ret.append(params.m_balance.str());
  arr_ret.append(Json::Value::UInt64(params.m_available_gas));

  return arr_ret;
}

bool EvmUtils::EvmUpdateContractStateAndAccount(
    Account* contractAccount, evmproj::ApplyInstructions& op) {
  if (op.OperationType() == "modify") {
    if (op.isResetStorage()) {
      contractAccount->SetStorageRoot(dev::h256());
    }
    if (op.Code().size() > 0)
      contractAccount->SetCode(DataConversion::StringToCharArray(op.Code()));

    for (const auto& it : op.Storage()) {
      if (!Contract::ContractStorage::GetContractStorage().UpdateStateValue(
              Address(op.Address()),
              DataConversion::StringToCharArray(it.Key()), 0,
              DataConversion::StringToCharArray(it.Value()), 0)) {
        return false;
      }
    }

    if (op.Balance().size()) {
      contractAccount->SetBalance(uint128_t(op.Balance()));
    }

    if (op.Nonce().size()) {
      contractAccount->SetNonce(std::stoull(op.Nonce()));
    }
  }
  return true;
}

uint64_t EvmUtils::UpdateGasRemaining(TransactionReceipt& receipt,
                                      INVOKE_TYPE invoke_type,
                                      uint64_t& oldValue, uint64_t newValue) {
  uint64_t cost{0};

  if (newValue > 0) oldValue = std::min(oldValue, newValue);

  // Create has already been charged before we were invoked.
  if (invoke_type == RUNNER_CREATE) return oldValue;

  cost = CONTRACT_INVOKE_GAS;

  if (oldValue > cost) {
    oldValue -= cost;
  } else {
    oldValue = 0;
    receipt.AddError(NO_GAS_REMAINING_FOUND);
  }
  LOG_GENERAL(INFO, "gasRemained: " << oldValue);

  return oldValue;
}