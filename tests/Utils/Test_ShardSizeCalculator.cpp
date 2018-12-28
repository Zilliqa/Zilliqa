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

#include "common/Constants.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/ShardSizeCalculator.h"

#define BOOST_TEST_MODULE ShardSizeCalculator
#define BOOST_TEST_DYN_LINK
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
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

  const uint32_t shardSize = 20;
  const uint32_t shardSizeThresholdLo = 10;
  const uint32_t shardSizeThresholdHi = 0;
  vector<uint32_t> shardCounts;

  for (uint32_t numNodesForSharding = 0; numNodesForSharding <= (shardSize * 4);
       numNodesForSharding++) {
    LOG_GENERAL(INFO, "Testing node count = " << numNodesForSharding);
    ShardSizeCalculator::GenerateShardCounts(shardSize, shardSizeThresholdLo,
                                             shardSizeThresholdHi,
                                             numNodesForSharding, shardCounts);
    LOG_GENERAL(INFO, "================================");
  }

  const uint32_t shardSizeThresholdLo2 = 5;
  const uint32_t shardSizeThresholdHi2 = 5;

  for (uint32_t numNodesForSharding = 0; numNodesForSharding <= (shardSize * 4);
       numNodesForSharding++) {
    LOG_GENERAL(INFO, "Testing node count = " << numNodesForSharding);
    ShardSizeCalculator::GenerateShardCounts(shardSize, shardSizeThresholdLo2,
                                             shardSizeThresholdHi2,
                                             numNodesForSharding, shardCounts);
    LOG_GENERAL(INFO, "================================");
  }
}

BOOST_AUTO_TEST_SUITE_END()
