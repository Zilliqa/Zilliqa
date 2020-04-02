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

#include "CoinbaseStruct.h"

using namespace std;

CoinbaseStruct::CoinbaseStruct() {}

CoinbaseStruct::CoinbaseStruct(const uint64_t& blockNumberInput,
                               const int32_t& shardIdInput,
                               const std::vector<bool>& b1Input,
                               const std::vector<bool>& b2Input,
                               const uint128_t& rewardsInput)
    : m_blockNumber(blockNumberInput),
      m_shardId(shardIdInput),
      m_b1(b1Input),
      m_b2(b2Input),
      m_rewards(rewardsInput) {}

/// Returns the current block number.
const uint64_t& CoinbaseStruct::GetBlockNumber() const { return m_blockNumber; }

/// Returns the shard id.
const int32_t& CoinbaseStruct::GetShardId() const { return m_shardId; }

/// Returns the b1
const vector<bool>& CoinbaseStruct::GetB1() const { return m_b1; }

//// Returns the b2
const vector<bool>& CoinbaseStruct::GetB2() const { return m_b2; }

/// Returns the rewards
const uint128_t& CoinbaseStruct::GetRewards() const { return m_rewards; }
