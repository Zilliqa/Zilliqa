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

#include "RewardControlContractState.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountStore/AccountStore.h"
#include "libUtils/JsonUtils.h"
#include <shared_mutex>

using namespace std;

namespace {
bool GetUint128FromState(Account& account, const std::string& key,uint128_t& val) {
  Json::Value root;
  std::vector<string> indices;
  const JSONUtils &utils = JSONUtils::GetInstance();
  if (account.FetchStateJson(root, key, indices)) {
    return utils.getUint128FromObject(root, key, val);
  } else {
    return false;
  }
}
}


RewardControlContractState RewardControlContractState::GetCurrentRewards() {
  RewardControlContractState parsed_state(
      COINBASE_REWARD_PER_DS,
      BASE_REWARD_IN_PERCENT,
      LOOKUP_REWARD_IN_PERCENT,
      1);

  LOG_GENERAL(INFO, "RCA " << REWARD_CONTROL_CONTRACT_ADDRESS);
  // Find the address, if there is one
  Address myAddress;
  if (ToBase16Addr(REWARD_CONTROL_CONTRACT_ADDRESS, myAddress)== AddressConversionCode::OK) {
    shared_lock<shared_timed_mutex> lock(AccountStore::GetInstance().GetPrimaryMutex());
    Account *rewardContract = AccountStore::GetInstance().GetAccount(myAddress);
    if (rewardContract && rewardContract->isContract()) {
    if (rewardContract != nullptr) {
      Json::Value root;
      // Make sure the fetch is atomic.
      RewardControlContractState maybe_parsed = parsed_state;
      if (maybe_parsed.FromAccount(*rewardContract)) {
        LOG_GENERAL(INFO, "RCA: State parsed correctly");
        parsed_state = maybe_parsed;
      } else {
        LOG_GENERAL(INFO, "RCA: Failed to parse state");
      }
    }
    } else {
      LOG_GENERAL(INFO, "RCA: Is not a contract");
    }
  } else {
    LOG_GENERAL(INFO, "RCA is not an address");
  }
  LOG_GENERAL(INFO, "Reward control state "
              << "B:" << parsed_state.base_reward_in_percent
              << " L:" << parsed_state.lookup_reward_in_percent
              << " P: " << parsed_state.percent_prec
              << " T:" << parsed_state.coinbase_reward_per_ds);
  return parsed_state;
}

bool RewardControlContractState::FromAccount(Account& account) {
  // Must be an object.
  bool ok = true;
  const JSONUtils& utils = JSONUtils::GetInstance();

  ok = ok && GetUint128FromState(account, "base_reward_in_percent", base_reward_in_percent);
  ok = ok && GetUint128FromState(account, "lookup_reward_in_percent", lookup_reward_in_percent);
  ok = ok && GetUint128FromState(account, "coinbase_reward_per_ds", coinbase_reward_per_ds);
  ok = ok && GetUint128FromState(account, "percent_precision", percent_prec);
  return ok;
}

