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
    Account* contractAccount, evmproj::ApplyInstructions& op) {
  if (op.OperationType() == "modify") {
    // Reset State for this contract
    try {
      if (op.isResetStorage()) {
        //
        // Reset Meta Data for this Address effectively clears down the contract
        // storage for this Contract
        //
        std::map<std::string, bytes> t_metadata;

        t_metadata.emplace(
            Contract::ContractStorage::GenerateStorageKey(
                Address(op.Address()), CONTRACT_ADDR_INDICATOR, {}),
            Address(op.Address()).asBytes());

        if (!contractAccount->UpdateStates(Address(op.Address()), t_metadata,
                                           {}, true)) {
          LOG_GENERAL(WARNING,
                      "Account::UpdateStates reset metaData and Merkyle tree");
        }
        contractAccount->SetStorageRoot(dev::h256());
      }
    } catch (std::exception& e) {
      // for now catch any generic exceptions and report them
      // will exmine exact possibilities and catch specific exceptions.
      LOG_GENERAL(WARNING,
                  "Exception thrown trying to reset storage " << e.what());
    }
    // If Instructed to reset the Code do so and call SetImmutable to reset
    // the hash
    try {
      if (op.Code().size() > 0)
        contractAccount->SetImmutable(
            DataConversion::StringToCharArray("EVM" + op.Code()),
            contractAccount->GetInitData());
    } catch (std::exception& e) {
      // for now catch any generic exceptions and report them
      // will exmine exact possibilities and catch specific exceptions.
      LOG_GENERAL(WARNING, "Exception thrown trying to update Contract code "
                               << e.what());
    }
    // Actually Update the state for the contract
    try {
      for (const auto& it : op.Storage()) {
        if (!Contract::ContractStorage::GetContractStorage().UpdateStateValue(
                Address(op.Address()),
                DataConversion::StringToCharArray(it.Key()), 0,
                DataConversion::StringToCharArray(it.Value()), 0)) {
          return false;
        }
      }
    } catch (std::exception& e) {
      // for now catch any generic exceptions and report them
      // will exmine exact possibilities and catch specific exceptions.
      LOG_GENERAL(WARNING,
                  "Exception thrown trying to update state on the contract "
                      << e.what());
    }
    try {
      if (op.Balance().size())
        contractAccount->SetBalance(uint128_t(op.Balance()));
    } catch (std::exception& e) {
      // for now catch any generic exceptions and report them
      // will exmine exact possibilities and catch specific exceptions.
      LOG_GENERAL(
          WARNING,
          "Exception thrown trying to update balance on contract Account "
              << e.what());
    }
    try {
      if (op.Nonce().size()) contractAccount->SetNonce(std::stoull(op.Nonce()));
    } catch (std::exception& e) {
      // for now catch any generic exceptions and report them
      // will exmine exact possibilities and catch specific exceptions.
      LOG_GENERAL(WARNING,
                  "Exception thrown trying to set Nonce on contract Account "
                      << e.what());
    }
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
    LOG_GENERAL(WARNING, "EVM is set and Code is empty, logic error");
    // returning false which means it will behave as if it was a scilla only
    // TODO : handle this third state
    return false;
  }

  if (code.size() < 4) return false;
  return (code[0] == 'E' && code[1] == 'V' && code[2] == 'M');
}
