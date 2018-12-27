/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
    const uint32_t shardSize, const uint32_t shardSizeThreshold,
    const uint32_t numNodesForSharding, vector<uint32_t>& shardCounts,
    bool logDetails) {
  LOG_MARKER();

  const uint32_t SHARD_THRESHOLD_LO = shardSize - shardSizeThreshold;
  const uint32_t SHARD_THRESHOLD_HI = shardSize + shardSizeThreshold;

  if (logDetails) {
    LOG_GENERAL(INFO, "Default shard size          = " << shardSize);
    LOG_GENERAL(INFO, "Minimum allowed shard size  = " << SHARD_THRESHOLD_LO);
    LOG_GENERAL(INFO, "Maximum allowed shard size  = " << SHARD_THRESHOLD_HI);
  }

  // Abort if total number of nodes is below SHARD_THRESHOLD_LO
  if (numNodesForSharding < SHARD_THRESHOLD_LO) {
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
  } else if (numUnshardedNodes < SHARD_THRESHOLD_LO) {
    // If remaining count is less than SHARD_THRESHOLD_LO, distribute among the
    // shards
    for (auto& shardCount : shardCounts) {
      // Add just enough nodes to each shard such that we don't go over
      // SHARD_THRESHOLD_HI
      const uint32_t nodesToAdd =
          min(SHARD_THRESHOLD_HI - shardCount, numUnshardedNodes);
      shardCount += nodesToAdd;
      numUnshardedNodes -= nodesToAdd;

      if (numUnshardedNodes == 0) {
        break;
      }
    }
  } else {
    // If remaining count is greater than or equal to SHARD_THRESHOLD_LO, allow
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
    const uint32_t shardSize, const uint32_t shardSizeThreshold,
    const uint32_t numNodesForSharding) {
  LOG_MARKER();

  vector<uint32_t> shardCounts;

  GenerateShardCounts(shardSize, shardSizeThreshold, numNodesForSharding,
                      shardCounts, false);

  if (shardCounts.empty()) {
    return numNodesForSharding;
  }

  uint32_t trimmedCount = 0;
  for (const auto& shardCount : shardCounts) {
    trimmedCount += shardCount;
  }

  return trimmedCount;
}