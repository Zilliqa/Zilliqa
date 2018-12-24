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

#ifndef __SHARD_SIZE_CALCULATOR_H__
#define __SHARD_SIZE_CALCULATOR_H__

#include <boost/algorithm/hex.hpp>

class ShardSizeCalculator {
 public:
  /// Calculate and return the min size of required each shard for a specifc
  /// number of total nodes
  static uint32_t CalculateShardSize(const uint32_t numberOfNodes);

  static void GenerateShardCounts(const uint32_t shardSize,
                                  const uint32_t shardSizeThreshold,
                                  const uint32_t numNodesForSharding,
                                  std::vector<uint32_t>& shardCounts);
};

#endif  // __SHARD_SIZE_CALCULATOR_H__