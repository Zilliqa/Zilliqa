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

#ifndef ZILLIQA_SRC_LIBUTILS_SHARDSIZECALCULATOR_H_
#define ZILLIQA_SRC_LIBUTILS_SHARDSIZECALCULATOR_H_

#include <boost/algorithm/hex.hpp>
#include <vector>

class ShardSizeCalculator {
 public:
  /// Calculate and return the min size of required each shard for a specifc
  /// number of total nodes
  static uint32_t CalculateShardSize(const uint32_t numberOfNodes);
};

#endif  // ZILLIQA_SRC_LIBUTILS_SHARDSIZECALCULATOR_H_
