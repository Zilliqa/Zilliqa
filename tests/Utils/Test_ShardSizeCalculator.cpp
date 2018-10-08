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

#include "libUtils/Logger.h"
#include "libUtils/ShardSizeCalculator.h"

#define BOOST_TEST_MODULE ShardSizeCalculator
#define BOOST_TEST_DYN_LINK
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(shardsizecalculator)

BOOST_AUTO_TEST_CASE(test_lower_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(100);
  BOOST_CHECK_MESSAGE(result == 651,
                      "Expected: 651. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_one_shard_normal) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(700);
  BOOST_CHECK_MESSAGE(result == 651,
                      "Expected: 651. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_one_shard_lower_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(651);
  BOOST_CHECK_MESSAGE(result == 651,
                      "Expected: 651. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_one_shard_upper_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(1367);
  BOOST_CHECK_MESSAGE(result == 651,
                      "Expected: 651. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_two_shards_lower_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(1368);
  BOOST_CHECK_MESSAGE(result == 684,
                      "Expected: 684. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_two_shards_upper_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(2131);
  BOOST_CHECK_MESSAGE(result == 684,
                      "Expected: 651. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_ten_shards) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(8000);
  BOOST_CHECK_MESSAGE(result == 771,
                      "Expected: 771. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_ten_shards_lower_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(7710);
  BOOST_CHECK_MESSAGE(result == 771,
                      "Expected: 771. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_ten_shards_upper_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(8579);
  BOOST_CHECK_MESSAGE(result == 771,
                      "Expected: 771. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_normal) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(20000);
  BOOST_CHECK_MESSAGE(result == 816,
                      "Expected: 816. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_lower_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(19584);
  BOOST_CHECK_MESSAGE(result == 816,
                      "Expected: 816. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_upper_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(20399);
  BOOST_CHECK_MESSAGE(result == 816,
                      "Expected: 816. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_exceed_lower_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(20400);
  BOOST_CHECK_MESSAGE(result == 819,
                      "Expected: 816. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_exceed_normal) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(1000000);
  BOOST_CHECK_MESSAGE(result == 819,
                      "Expected: 816. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_exceed_upper_bound_minus_one) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(
      std::numeric_limits<uint32_t>::max() - 1);
  BOOST_CHECK_MESSAGE(result == 819,
                      "Expected: 816. Result: " + to_string(result));
}

BOOST_AUTO_TEST_CASE(test_max_shards_exceed_upper_bound) {
  INIT_STDOUT_LOGGER();
  uint32_t result = ShardSizeCalculator::CalculateShardSize(
      std::numeric_limits<uint32_t>::max());
  BOOST_CHECK_MESSAGE(result == 819,
                      "Expected: 816. Result: " + to_string(result) + " " +
                          to_string(std::numeric_limits<uint32_t>::max()));
}

BOOST_AUTO_TEST_SUITE_END()
