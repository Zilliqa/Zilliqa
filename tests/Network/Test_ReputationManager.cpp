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

#include "libNetwork/ReputationManager.h"

#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE ReputationManager
#define BOOST_TEST_DYN_LINK
#include <arpa/inet.h>
#include <limits.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(reputationManager)

boost::multiprecision::uint128_t node1, node2;

void setup() {
  // INIT_STDOUT_LOGGER();

  ReputationManager& rm = ReputationManager::GetInstance();
  rm.Clear();

  node1 = IPConverter::ToNumericalIPFromStr("127.0.0.1");
  node2 = IPConverter::ToNumericalIPFromStr("192.168.1.1");
  // Setup
  rm.AddNodeIfNotKnown(node1);
  rm.AddNodeIfNotKnown(node2);
}

void tearDown() {
  ReputationManager& rm = ReputationManager::GetInstance();
  rm.Clear();
}

void banNode1() {
  ReputationManager& rm = ReputationManager::GetInstance();
  rm.PunishNode(node1, ReputationManager::ScoreType::REPTHRESHOLD - 1);
}

BOOST_AUTO_TEST_CASE(test_check_new_node_rep) {
  setup();
  ReputationManager& rm = ReputationManager::GetInstance();

  // Check reputation of new node
  int32_t result = rm.GetReputation(node1);
  int32_t expected = 0;
  BOOST_CHECK_MESSAGE(
      result == expected,
      "Check reputation of new node. Result: " + to_string(result) +
          ". Expected: " + to_string(expected) + ".");

  tearDown();
}

BOOST_AUTO_TEST_CASE(test_check_new_node_ban_status) {
  setup();
  ReputationManager& rm = ReputationManager::GetInstance();
  bool result_bool = rm.IsNodeBanned(node1);
  BOOST_CHECK_MESSAGE(result_bool == false,
                      "Test ban for unban node. Expected: false");
  tearDown();
}

BOOST_AUTO_TEST_CASE(test_banned_node_status) {
  setup();
  banNode1();

  ReputationManager& rm = ReputationManager::GetInstance();
  rm.PunishNode(node1, ReputationManager::ScoreType::REPTHRESHOLD - 1);
  BOOST_CHECK_MESSAGE(rm.IsNodeBanned(node1) == true,
                      "Test ban for banned node. Expected: true");
  tearDown();
}

BOOST_AUTO_TEST_CASE(test_banned_node_rep) {
  setup();
  banNode1();

  ReputationManager& rm = ReputationManager::GetInstance();
  BOOST_CHECK_MESSAGE(rm.IsNodeBanned(node1) == true,
                      "Test ban for banned node. Expected: true");

  int32_t result = rm.GetReputation(node1);
  int32_t expected = 0 + ReputationManager::ScoreType::REPTHRESHOLD - 1 -
                     (ReputationManager::ScoreType::BAN_MULTIPLIER *
                      ReputationManager::ScoreType::AWARD_FOR_GOOD_NODES);

  BOOST_CHECK_MESSAGE(result == expected,
                      "Check rep score after getting banned. Result: "
                          << result << ". Expected: " << expected);

  tearDown();
}

BOOST_AUTO_TEST_CASE(test_banned_node_status_after_reward) {
  setup();
  banNode1();

  ReputationManager& rm = ReputationManager::GetInstance();
  rm.AwardAllNodes();

  BOOST_CHECK_MESSAGE(
      rm.IsNodeBanned(node1) == true,
      IPConverter::ToStrFromNumericalIP(node1)
          << " ban status after 1 round of reward) expected to be "
             "true but false was obtained.");
  tearDown();
}

BOOST_AUTO_TEST_CASE(test_unban_node_status_after_reward) {
  setup();
  banNode1();

  ReputationManager& rm = ReputationManager::GetInstance();
  rm.AwardAllNodes();

  BOOST_CHECK_MESSAGE(
      rm.IsNodeBanned(node2) == false,
      IPConverter::ToStrFromNumericalIP(node2)
          << " ban status after 1 round of reward) expected to be "
             "false but true was obtained.");

  tearDown();
}

BOOST_AUTO_TEST_CASE(test_node_unbanning) {
  setup();
  banNode1();

  ReputationManager& rm = ReputationManager::GetInstance();
  rm.AwardAllNodes();

  // Reward till node unban and check status of unban node
  for (int i = 0; i < ReputationManager::ScoreType::BAN_MULTIPLIER; i++) {
    rm.AwardAllNodes();
  }
  BOOST_CHECK_MESSAGE(rm.IsNodeBanned(node1) == false,
                      "Test ban for unban node. Expected: false");
  tearDown();
}

BOOST_AUTO_TEST_CASE(test_rep_upperbound) {
  setup();
  ReputationManager& rm = ReputationManager::GetInstance();
  // Reward till node unban and check status of unban node
  for (int i = 0; i < ReputationManager::ScoreType::BAN_MULTIPLIER; i++) {
    rm.AwardAllNodes();
  }
  int32_t result = rm.GetReputation(node2);
  int32_t expected = ReputationManager::ScoreType::UPPERREPTHRESHOLD;

  BOOST_CHECK_MESSAGE(result == expected, "Upper bound of reputation test: "
                                              << result
                                              << ". Expected: " << expected);
  tearDown();
}

BOOST_AUTO_TEST_CASE(test_rep_underflow) {
  setup();
  banNode1();

  ReputationManager& rm = ReputationManager::GetInstance();

  // Attempt to underflow the rep. Expected result: Nothing will be change.
  int32_t expected = rm.GetReputation(node1);
  rm.PunishNode(node1, std::numeric_limits<int32_t>::min());
  int32_t result = rm.GetReputation(node1);

  BOOST_CHECK_MESSAGE(
      result == expected,
      "rep underflow test. Result: " << result << ". Expected: " << expected);

  tearDown();
}

BOOST_AUTO_TEST_SUITE_END()
