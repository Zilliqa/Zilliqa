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

#include "ShardSizeCalculator.h"
#include "libUtils/Logger.h"

using namespace std;

uint32_t ShardSizeCalculator::CalculateShardSize(const uint32_t numberOfNodes) {
  // Zilliqa can support up till 25 shards at max. After which, shard nodes need
  // to compete hard to join shard.
  if (numberOfNodes >= 21294) {
    LOG_GENERAL(INFO, "Max number of shards reached.");
    return 819;
  }

  static uint32_t range[] = {0,     651,   1368,
                             2133,  2868,  3675,
                             4464,  5229,  6024,
                             6858,  7710,  8580,
                             9468,  10335, 11130,
                             11925, 12720, 13515,
                             14364, 15390, 16200,
                             17010, 17820, 18768,
                             19584, 20400, numeric_limits<uint32_t>::max()};

  // result[0] will never be used
  static uint32_t result[] = {0,   651, 651, 684, 711, 717, 735, 744, 747,
                              753, 762, 771, 780, 789, 795, 795, 795, 795,
                              795, 798, 810, 810, 810, 810, 816, 816, 819};

  auto constexpr range_size = extent<decltype(range)>::value;
  auto constexpr result_size = extent<decltype(result)>::value;

  static_assert(range_size == result_size,
                "mismatch in static shard size table");

  auto it = upper_bound(range, range + range_size, numberOfNodes);
  size_t index = it - range;

  return result[index];
}

void ShardSizeCalculator::GenerateShardCounts(
    const uint32_t shardSize, const uint32_t shardSizeToleranceLo,
    const uint32_t shardSizeToleranceHi, const uint32_t numNodesForSharding,
    vector<uint32_t>& shardCounts, bool logDetails) {
  LOG_MARKER();

  if (shardSizeToleranceLo >= shardSize) {
    LOG_GENERAL(
        WARNING,
        "SHARD_SIZE_TOLERANCE_LO must be smaller than current shard size!");
  }

  const uint32_t shard_threshold_lo = shardSize - shardSizeToleranceLo;
  const uint32_t shard_threshold_hi = shardSize + shardSizeToleranceHi;

  if (logDetails) {
    LOG_GENERAL(INFO, "Default shard size          = " << shardSize);
    LOG_GENERAL(INFO, "Minimum allowed shard size  = " << shard_threshold_lo);
    LOG_GENERAL(INFO, "Maximum allowed shard size  = " << shard_threshold_hi);
  }

  // Abort if total number of nodes is below shard_threshold_lo
  if (numNodesForSharding < shard_threshold_lo) {
    if (logDetails) {
      LOG_GENERAL(WARNING, "Number of PoWs for sharding ("
                               << numNodesForSharding
                               << ") is not enough for even one shard.");
    }
    return;
  }

  // Get the number of full shards that can be formed
  const uint32_t numOfCompleteShards = numNodesForSharding / shardSize;

  if (numOfCompleteShards == 0) {
    // If can't form one full shard, set first shard count to 0
    shardCounts.resize(1);
    shardCounts.at(0) = 0;
  } else {
    // If can form one or more full shards, set shard count to shardSize
    shardCounts.resize(numOfCompleteShards);
    fill(shardCounts.begin(), shardCounts.end(), shardSize);
  }

  // Get the remaining count of unsharded nodes
  uint32_t numUnshardedNodes =
      numNodesForSharding - (numOfCompleteShards * shardSize);

  if (numUnshardedNodes == numNodesForSharding) {
    // Remaining count = original node count -> set first shard count to
    // remaining count
    shardCounts.at(0) = numUnshardedNodes;
  } else if ((numUnshardedNodes < shard_threshold_lo) &&
             (shard_threshold_hi > 0)) {
    // If remaining count is less than shard_threshold_lo, distribute among the
    // shards
    for (auto& shardCount : shardCounts) {
      // Add just enough nodes to each shard such that we don't go over
      // shard_threshold_hi
      const uint32_t nodesToAdd =
          min(shard_threshold_hi - shardCount, numUnshardedNodes);
      shardCount += nodesToAdd;
      numUnshardedNodes -= nodesToAdd;

      if (numUnshardedNodes == 0) {
        break;
      }
    }
  } else {
    // If remaining count is greater than or equal to shard_threshold_lo, allow
    // formation of another shard
    shardCounts.emplace_back(numUnshardedNodes);
  }

  if (logDetails) {
    LOG_GENERAL(INFO, "Final computed shard sizes:");
    for (unsigned int i = 0; i < shardCounts.size(); i++) {
      LOG_GENERAL(INFO, "Shard " << i << " = " << shardCounts.at(i));
    }
  }
}

uint32_t ShardSizeCalculator::GetTrimmedShardCount(
    const uint32_t shardSize, const uint32_t shardSizeToleranceLo,
    const uint32_t shardSizeToleranceHi, const uint32_t numNodesForSharding) {
  LOG_MARKER();

  vector<uint32_t> shardCounts;

  GenerateShardCounts(shardSize, shardSizeToleranceLo, shardSizeToleranceHi,
                      numNodesForSharding, shardCounts, false);

  if (shardCounts.empty()) {
    return numNodesForSharding;
  }

  uint32_t trimmedCount = 0;
  for (const auto& shardCount : shardCounts) {
    trimmedCount += shardCount;
  }

  return trimmedCount;
}
