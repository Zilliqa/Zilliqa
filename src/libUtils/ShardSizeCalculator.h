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

#ifndef __SHARD_SIZE_CALCULATOR_H__
#define __SHARD_SIZE_CALCULATOR_H__

#include <boost/algorithm/hex.hpp>
#include <vector>

class ShardSizeCalculator {
 public:
  /// Calculate and return the min size of required each shard for a specifc
  /// number of total nodes
  static uint32_t CalculateShardSize(const uint32_t numberOfNodes);

  static void GenerateShardCounts(const uint32_t shardSize,
                                  const uint32_t shardSizeToleranceLo,
                                  const uint32_t shardSizeToleranceHi,
                                  const uint32_t numNodesForSharding,
                                  std::vector<uint32_t>& shardCounts,
                                  bool logDetails = true);

  static uint32_t GetTrimmedShardCount(const uint32_t shardSize,
                                       const uint32_t shardSizeToleranceLo,
                                       const uint32_t shardSizeToleranceHi,
                                       const uint32_t numNodesForSharding);
};

#endif  // __SHARD_SIZE_CALCULATOR_H__
