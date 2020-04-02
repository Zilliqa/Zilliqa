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

#ifndef ZILLIQA_SRC_LIBDATA_COINBASEDATA_COINBASESTRUCT_H_
#define ZILLIQA_SRC_LIBDATA_COINBASEDATA_COINBASESTRUCT_H_

#include <array>

#include "common/Constants.h"

/// Holds information of cosigs and rewards for specific block and specific
/// shard.
class CoinbaseStruct {
  uint64_t m_blockNumber{};
  int32_t m_shardId{};
  std::vector<bool> m_b1;
  std::vector<bool> m_b2;
  uint128_t m_rewards{};

 public:
  /// Default constructor.
  CoinbaseStruct();

  /// Constructor with specified transaction fields.
  CoinbaseStruct(const uint64_t& blockNumberInput,
                 const int32_t& m_shardIdInput,
                 const std::vector<bool>& b1Input,
                 const std::vector<bool>& b2Input,
                 const uint128_t& rewardsInput);

  /// Returns the current block number.
  const uint64_t& GetBlockNumber() const;

  /// Returns the shard id.
  const int32_t& GetShardId() const;

  /// Returns the b1.
  const std::vector<bool>& GetB1() const;

  /// Returns the b2.
  const std::vector<bool>& GetB2() const;

  /// Returns the rewards.
  const uint128_t& GetRewards() const;
};

#endif  // ZILLIQA_SRC_LIBDATA_COINBASEDATA_COINBASESTRUCT_H_
