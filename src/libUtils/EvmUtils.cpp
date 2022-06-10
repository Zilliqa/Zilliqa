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
#include "libPersistence/ContractStorage.h"
#include "libServer/ScillaIPCServer.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"

using namespace std;
using namespace boost::multiprecision;

Json::Value EvmUtils::GetEvmCallJson(const EvmCallParameters& params) {
  Json::Value arr_ret(Json::arrayValue);

  arr_ret.append(params.m_contract);
  arr_ret.append(params.m_caller);
  std::string code;
  try {
    // take off the EVM prefix
    std::copy(params.m_code.begin() + 3, params.m_code.end(),
              std::back_inserter(code));
    arr_ret.append(code);
  } catch (std::exception& e) {
    arr_ret.append(params.m_code);
  }
  arr_ret.append(params.m_data);
  arr_ret.append(params.m_apparent_value.str());
  arr_ret.append(Json::Value::UInt64(params.m_available_gas));

  if (LOG_SC) {
    LOG_GENERAL(WARNING, "Sending to EVM-DS" << arr_ret);
  }

  return arr_ret;
}

bool EvmUtils::EvmUpdateContractStateAndAccount(
    std::shared_ptr<ScillaIPCServer> ipcServer, Account* contractAccount,
    evmproj::ApplyInstructions& op) {
  if (op.OperationType() == "modify") {
    if (op.isResetStorage()) contractAccount->SetStorageRoot(dev::h256());

    if (op.Code().size() > 0)
      contractAccount->SetImmutable(
          DataConversion::StringToCharArray("EVM" + op.Code()),
          contractAccount->GetInitData());
    if (ipcServer)
      for (const auto& it : op.Storage())
        if (!ipcServer->updateStateValue(it.Key(), it.Value()))
          LOG_GENERAL(INFO, "Updated State and Value for " << it.Key());

    if (op.Balance().size())
      contractAccount->SetBalance(uint128_t(op.Balance()));

    if (op.Nonce().size()) contractAccount->SetNonce(std::stoull(op.Nonce()));
  }
  return true;
}

uint64_t EvmUtils::UpdateGasRemaining(TransactionReceipt& receipt,
                                      INVOKE_TYPE invoke_type,
                                      uint64_t& oldValue, uint64_t newValue) {
  uint64_t cost{0};

  if (newValue > 0) oldValue = std::min(oldValue, newValue);

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

bool EvmUtils::isEvm(const bytes& code) {
  if (not ENABLE_EVM) return false;

  if (code.empty()) {
    LOG_GENERAL(WARNING, "Logic error code cannot be empty");
    std::terminate();
  }

  if (code.size() < 4) return false;
  return (code[0] == 'E' && code[1] == 'V' && code[2] == 'M');
}
