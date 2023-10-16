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

#ifndef ZILLIQA_SRC_LIBDATA_COINBASEDATA_REWARDCONTROLCONTRACTSTATE_H_
#define ZILLIQA_SRC_LIBDATA_COINBASEDATA_REWARDCONTROLCONTRACTSTATE_H_

#include "common/BaseType.h"
#include "libData/AccountData/Account.h"

// The numbers here are represented as strings because that is how they are represented in contract
// state.
struct RewardControlContractState {
  uint128_t coinbase_reward_per_ds;
  uint128_t base_reward_in_percent;
  uint128_t lookup_reward_in_percent;
  uint128_t percent_prec;

  RewardControlContractState(uint128_t coinbase_reward_per_ds_,
                       uint128_t base_reward_in_percent_,
                       uint128_t lookup_reward_in_percent_,
                       uint128_t percent_prec_) :
      coinbase_reward_per_ds(coinbase_reward_per_ds_),
      base_reward_in_percent(base_reward_in_percent_),
      lookup_reward_in_percent(lookup_reward_in_percent_),
      percent_prec(percent_prec_) { }

  /// returns true if we succeeded, false if we didn't.
  /// WARNING: Will change this even if it returns false! Be careful.
  bool FromAccount(Account& account);

  static RewardControlContractState GetCurrentRewards();
};



#endif  // ZILLIQA_SRC_LIBDATA_COINBASEDATA_REWARDCONTROLCONTRACTSTATE_H_
