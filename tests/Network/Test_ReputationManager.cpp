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

#include "libNetwork/ReputationManager.h"

#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE ReputationManager
#define BOOST_TEST_DYN_LINK
#include <arpa/inet.h>
#include <limits.h>
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(reputationManager)

uint128_t node1, node2;

void setup() {
  // INIT_STDOUT_LOGGER();

  ReputationManager& rm = ReputationManager::GetInstance();
  rm.Clear();

  BOOST_CHECK_MESSAGE(IPConverter::ToNumericalIPFromStr("127.0.0.1", node1),
                      "Conversion from IP "
                          << "127.0.0.1"
                          << " to integer failed.");
  BOOST_CHECK_MESSAGE(IPConverter::ToNumericalIPFromStr("192.168.1.1", node2),
                      "Conversion from IP "
                          << "192.168.1.1"
                          << " to integer failed.");
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
