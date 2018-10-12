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

uint32_t ShardSizeCalculator::CalculateShardSize(const uint32_t numberOfNodes) {
  // Zilliqa can support up till 25 shards at max. After which, shard nodes need
  // to compete hard to join shard.
  if (numberOfNodes >= 21294) {
    LOG_GENERAL(INFO, "Max number of shards reached.");
    return 819;
  }

  static uint32_t range[] = {
      0,     651,   1368,
      2133,  2868,  3675,
      4464,  5229,  6024,
      6858,  7710,  8580,
      9468,  10335, 11130,
      11925, 12720, 13515,
      14364, 15390, 16200,
      17010, 17820, 18768,
      19584, 20400, std::numeric_limits<uint32_t>::max()};

  // result[0] will never be used
  static uint32_t result[] = {0,   651, 651, 684, 711, 717, 735, 744, 747,
                              753, 762, 771, 780, 789, 795, 795, 795, 795,
                              795, 798, 810, 810, 810, 810, 816, 816, 819};

  auto constexpr range_size = std::extent<decltype(range)>::value;
  auto constexpr result_size = std::extent<decltype(result)>::value;

  static_assert(range_size == result_size,
                "mismatch in static shard size table");

  auto it = std::upper_bound(range, range + range_size, numberOfNodes);
  std::size_t index = it - range;

  return result[index];
}
