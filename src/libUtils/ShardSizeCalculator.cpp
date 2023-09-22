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

