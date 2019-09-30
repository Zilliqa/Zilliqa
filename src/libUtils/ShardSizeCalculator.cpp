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

static void GenerateShardCountsCore(const vector<uint32_t>& shardSizeValues,
                                    uint32_t numNodesForSharding,
                                    vector<uint32_t>& currentResult,
                                    vector<uint32_t>& bestResult,
                                    uint32_t& leastWaste) {
  // If number of remaining unsharded nodes is less than minimum threshold
  if (numNodesForSharding < shardSizeValues[0]) {
    // Distribute these nodes among the existing shards
    if ((numNodesForSharding > 0) && (currentResult.size() > 0)) {
      const uint32_t upperLimit = shardSizeValues[shardSizeValues.size() - 1];
      uint32_t toAddPerShard = numNodesForSharding / currentResult.size();
      if ((numNodesForSharding % currentResult.size()) > 0) {
        toAddPerShard++;
      }
      for (auto& shardInCurrentResult : currentResult) {
        // Don't add more nodes than the max threshold
        uint32_t actualToAdd =
            min(toAddPerShard, upperLimit - shardInCurrentResult);
        actualToAdd = min(actualToAdd, numNodesForSharding);
        shardInCurrentResult += actualToAdd;
        numNodesForSharding -= actualToAdd;
      }
    }

    // If we don't have a best result yet, this one is the best for now
    if (bestResult.empty()) {
      bestResult = currentResult;
      leastWaste = numNodesForSharding;
    }
    // If we have a best result already from a previous recursion
    else {
      // If this current result is as good as the best one, but achieves it with
      // fewer shards
      if ((numNodesForSharding == leastWaste) &&
          (bestResult.size() > currentResult.size())) {
        bestResult = currentResult;
      }
      // If this current result is better than the best one
      else if (numNodesForSharding < leastWaste) {
        bestResult = currentResult;
        leastWaste = numNodesForSharding;
      }
    }
  }
  // If number of remaining unsharded nodes is still more than minimum threshold
  else {
    // Recursively try adding another shard with the 3 values (low threshold,
    // shard size, high threshold)
    for (const auto& value : shardSizeValues) {
      if (numNodesForSharding < value) {
        break;
      }
      currentResult.push_back(value);
      GenerateShardCountsCore(shardSizeValues, numNodesForSharding - value,
                              currentResult, bestResult, leastWaste);
      currentResult.pop_back();
    }
  }
}

void ShardSizeCalculator::GenerateShardCounts(
    const uint32_t shardSize, const uint32_t shardSizeToleranceLo,
    const uint32_t shardSizeToleranceHi, const uint32_t numNodesForSharding,
    vector<uint32_t>& shardCounts) {
  if (shardSizeToleranceLo >= shardSize) {
    LOG_GENERAL(
        WARNING,
        "SHARD_SIZE_TOLERANCE_LO must be smaller than current shard size!");
  }

  const uint32_t shard_threshold_lo = shardSize - shardSizeToleranceLo;
  const uint32_t shard_threshold_hi = shardSize + shardSizeToleranceHi;

  // Abort if total number of nodes is below shard_threshold_lo
  if (numNodesForSharding < shard_threshold_lo) {
    LOG_GENERAL(WARNING, "Number of PoWs for sharding ("
                             << numNodesForSharding
                             << ") is not enough for even one shard.");
    return;
  }

  const vector<uint32_t> shardSizeValues = {shard_threshold_lo, shardSize,
                                            shard_threshold_hi};
  vector<uint32_t> currentResult;
  uint32_t leastWaste = numNodesForSharding;
  GenerateShardCountsCore(shardSizeValues, numNodesForSharding, currentResult,
                          shardCounts, leastWaste);
}

uint32_t ShardSizeCalculator::GetTrimmedShardCount(
    const uint32_t shardSize, const uint32_t shardSizeToleranceLo,
    const uint32_t shardSizeToleranceHi, const uint32_t numNodesForSharding) {
  LOG_MARKER();

  vector<uint32_t> shardCounts;

  GenerateShardCounts(shardSize, shardSizeToleranceLo, shardSizeToleranceHi,
                      numNodesForSharding, shardCounts);

  if (shardCounts.empty()) {
    return numNodesForSharding;
  }

  uint32_t trimmedCount = 0;
  for (const auto& shardCount : shardCounts) {
    trimmedCount += shardCount;
  }

  return trimmedCount;
}
