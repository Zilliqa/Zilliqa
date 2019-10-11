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

#include "common/Constants.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/ShardSizeCalculator.h"

#define BOOST_TEST_MODULE ShardSizeCalculator
#define BOOST_TEST_DYN_LINK

#include <array>
#include <boost/test/unit_test.hpp>
#include <map>
#include <vector>

using namespace std;

typedef std::vector<uint32_t> TestDataSet;
typedef std::map<uint32_t, TestDataSet> ShardSizeMap;

void prepareTestdata(ShardSizeMap& testData) {
  TestDataSet tds;
  testData[651] =
      tds = {0,   TestUtils::RandomIntInRng<uint32_t>(1, 649),    650,
             651, TestUtils::RandomIntInRng<uint32_t>(652, 1366), 1367};
  testData[684] =
      tds = {1368, TestUtils::RandomIntInRng<uint32_t>(1369, 2131), 2132};
  testData[711] =
      tds = {2133, TestUtils::RandomIntInRng<uint32_t>(2134, 2866), 2867};
  testData[717] =
      tds = {2868, TestUtils::RandomIntInRng<uint32_t>(2869, 3673), 3674};
  testData[735] =
      tds = {3675, TestUtils::RandomIntInRng<uint32_t>(3676, 4462), 4463};
  testData[744] =
      tds = {4464, TestUtils::RandomIntInRng<uint32_t>(4465, 5227), 5228};
  testData[747] =
      tds = {5229, TestUtils::RandomIntInRng<uint32_t>(5230, 6022), 6023};
  testData[753] =
      tds = {6024, TestUtils::RandomIntInRng<uint32_t>(6025, 6856), 6857};
  testData[762] =
      tds = {6858, TestUtils::RandomIntInRng<uint32_t>(6859, 7708), 7709};
  testData[771] =
      tds = {7710, TestUtils::RandomIntInRng<uint32_t>(7711, 8578), 8579};
  testData[780] =
      tds = {8580, TestUtils::RandomIntInRng<uint32_t>(8581, 9466), 9467};
  testData[789] =
      tds = {9468, TestUtils::RandomIntInRng<uint32_t>(9469, 10333), 10334};
  testData[795] =
      tds = {10335, TestUtils::RandomIntInRng<uint32_t>(10336, 14362), 14363};
  testData[798] =
      tds = {14364, TestUtils::RandomIntInRng<uint32_t>(14365, 15388), 15389};
  testData[810] =
      tds = {15390, TestUtils::RandomIntInRng<uint32_t>(15391, 18766), 18767};
  testData[816] =
      tds = {18768, TestUtils::RandomIntInRng<uint32_t>(18769, 19582), 19583};
  testData[816] =
      tds = {19584, TestUtils::RandomIntInRng<uint32_t>(19585, 20398), 20399};
  testData[819] =
      tds = {20400, TestUtils::RandomIntInRng<uint32_t>(20401, 21293), 21294,
             TestUtils::RandomIntInRng<uint32_t>(
                 21295, std::numeric_limits<uint32_t>::max() - 1),
             std::numeric_limits<uint32_t>::max()};
}

void ShardCountTestMain(const uint32_t shardSize,
                        const uint32_t shardSizeToleranceLo,
                        const uint32_t shardSizeToleranceHi,
                        const uint32_t nodeCountStart,
                        const uint32_t nodeCountEnd) {
  vector<uint32_t> shardCounts;

  for (uint32_t numNodesForSharding = nodeCountStart;
       numNodesForSharding <= nodeCountEnd; numNodesForSharding++) {
    ShardSizeCalculator::GenerateShardCounts(shardSize, shardSizeToleranceLo,
                                             shardSizeToleranceHi,
                                             numNodesForSharding, shardCounts);
    ostringstream shardsString;
    uint32_t totalSharded = 0;
    for (const auto& shard : shardCounts) {
      shardsString << shard << " ";
      totalSharded += shard;
    }
    LOG_GENERAL(
        INFO, "Shard lo,mid,hi=["
                  << shardSize - shardSizeToleranceLo << "," << shardSize << ","
                  << shardSize + shardSizeToleranceHi << "] Nodes="
                  << numNodesForSharding << " Shards=[ " << shardsString.str()
                  << "] Unsharded=" << numNodesForSharding - totalSharded);
    BOOST_CHECK(totalSharded <= numNodesForSharding);
  }
}

BOOST_AUTO_TEST_SUITE(shardsizecalculator)

#define TD_i td_i
#define EXPECTED td_i.first
#define NUMOFNODES_v td_i.second

BOOST_AUTO_TEST_CASE(test_shard_size_bounds) {
  INIT_STDOUT_LOGGER();
  ShardSizeMap testData;
  prepareTestdata(testData);
  for (auto const& TD_i : testData) {
    for (auto const& numOfNodes : NUMOFNODES_v) {
      uint32_t result = ShardSizeCalculator::CalculateShardSize(numOfNodes);
      BOOST_CHECK_MESSAGE(result == EXPECTED,
                          "For number of nodes: " + to_string(numOfNodes) +
                              " Expected: " + to_string(EXPECTED) +
                              ". Result: " + to_string(result));
    }
  }
}

// Right now the result for this test needs to be inspected visually
BOOST_AUTO_TEST_CASE(test_shard_count_generation) {
  INIT_STDOUT_LOGGER();

  // ShardCountTestMain(20, 10, 0, 0, 60);
  // ShardCountTestMain(20, 5, 5, 0, 60);
  // ShardCountTestMain(600, 0, 0, 590, 610);
  // ShardCountTestMain(600, 100, 0, 490, 610);
  // ShardCountTestMain(600, 50, 50, 540, 660);
  ShardCountTestMain(600, 100, 0, 490, 1810);
}

BOOST_AUTO_TEST_SUITE_END()
