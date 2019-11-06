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

#include <Schnorr.h>
#include <string>
#include "libData/BlockData/Block.h"
#include "libDirectoryService/DirectoryService.h"
#include "libNetwork/ShardStruct.h"
#include "libUtils/Logger.h"
#include "libUtils/SWInfo.h"

#define BOOST_TEST_MODULE determinebyzantinenodes
#define BOOST_TEST_DYN_LINK
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <boost/test/unit_test.hpp>

#define COMMITTEE_SIZE 20
#define NUM_OF_ELECTED 5
#define NUM_OF_REMOVED 3
#define LOCALHOST 0x7F000001
#define BASE_PORT 2600
#define NUM_OF_FINAL_BLOCK 100
#define STARTING_BLOCK 200
#define FINALBLOCK_REWARD -1
#define PERFORMANCE_THRESHOLD 0.25

using namespace std;

BOOST_AUTO_TEST_SUITE(determinebyzantinenodes)

struct F {
  F() {
    BOOST_TEST_MESSAGE("setup fixture");

    // Generate the self key.
    selfKeyPair = Schnorr::GenKeyPair();
    selfPubKey = selfKeyPair.second;

    // Generate the DS Committee.
    for (int i = 0; i < COMMITTEE_SIZE; ++i) {
      PairOfKey kp = Schnorr::GenKeyPair();
      PubKey pk = kp.second;
      Peer peer = Peer(LOCALHOST, BASE_PORT + i);
      PairOfNode entry = std::make_pair(pk, peer);
      dsComm.emplace_back(entry);
    }

    // Compute some default parameters
    maxCoSigs = (NUM_OF_FINAL_BLOCK - 1) * 2;
    threshold = std::ceil(PERFORMANCE_THRESHOLD * maxCoSigs);
  }

  ~F() { BOOST_TEST_MESSAGE("teardown fixture"); }

  PairOfKey selfKeyPair;
  PubKey selfPubKey;
  DequeOfNode dsComm;
  uint32_t maxCoSigs;
  uint32_t threshold;
};

// Test that performance is not taken into account on epoch 1.
BOOST_FIXTURE_TEST_CASE(test_EpochOne, F) {
  INIT_STDOUT_LOGGER();

  // Create the member performance.
  std::map<PubKey, uint32_t> dsMemberPerformance;
  for (const auto& member : dsComm) {
    // Definite non-performance.
    dsMemberPerformance[member.first] = 0;
  }

  // Initialise the removal list.
  std::vector<PubKey> removeDSNodePubkeys;

  unsigned int removeResult = DirectoryService::DetermineByzantineNodesCore(
      NUM_OF_ELECTED, removeDSNodePubkeys, 1, NUM_OF_FINAL_BLOCK,
      PERFORMANCE_THRESHOLD, NUM_OF_REMOVED, dsComm, dsMemberPerformance);

  // Check the size.
  BOOST_CHECK_MESSAGE(
      removeResult == 0,
      "removeResult value wrong. Actual: " << removeResult << ". Expected: 0.");
  BOOST_CHECK_MESSAGE(removeDSNodePubkeys.size() == 0,
                      "removeDSNodePubkeys size wrong. Actual: "
                          << removeDSNodePubkeys.size() << ". Expected: 0.");
}

// Test the case when there are no Byzantine nodes.
BOOST_FIXTURE_TEST_CASE(test_NoByzantineNodes, F) {
  INIT_STDOUT_LOGGER();

  // Create the member performance.
  std::map<PubKey, uint32_t> dsMemberPerformance;
  for (const auto& member : dsComm) {
    // Definite performance.
    dsMemberPerformance[member.first] = threshold + 1;
  }

  // Initialise the removal list.
  std::vector<PubKey> removeDSNodePubkeys;

  unsigned int removeResult = DirectoryService::DetermineByzantineNodesCore(
      NUM_OF_ELECTED, removeDSNodePubkeys, STARTING_BLOCK, NUM_OF_FINAL_BLOCK,
      PERFORMANCE_THRESHOLD, NUM_OF_REMOVED, dsComm, dsMemberPerformance);

  // Check the size.
  BOOST_CHECK_MESSAGE(
      removeResult == 0,
      "removeResult value wrong. Actual: " << removeResult << ". Expected: 0.");
  BOOST_CHECK_MESSAGE(removeDSNodePubkeys.size() == 0,
                      "removeDSNodePubkeys size wrong. Actual: "
                          << removeDSNodePubkeys.size() << ". Expected: 0.");
}

// Test the case when the number of Byzantine nodes is < maxByzantineRemoved.
BOOST_FIXTURE_TEST_CASE(test_LessThanByzantineNodes, F) {
  INIT_STDOUT_LOGGER();

  // Create the expected removed node list.
  std::vector<PubKey> expectedRemoveDSNodePubkeys;

  // Create the member performance.
  std::map<PubKey, uint32_t> dsMemberPerformance;
  unsigned int count = 0;
  unsigned int target = NUM_OF_REMOVED - 1;
  for (const auto& member : dsComm) {
    if (count < target) {
      dsMemberPerformance[member.first] = 0;
      expectedRemoveDSNodePubkeys.emplace_back(member.first);
    } else {
      dsMemberPerformance[member.first] = threshold + 1;
    }
    ++count;
  }

  // Check the expected list.
  BOOST_CHECK_MESSAGE(expectedRemoveDSNodePubkeys.size() == target,
                      "expectedRemoveDSNodePubkeys size wrong. Actual: "
                          << expectedRemoveDSNodePubkeys.size()
                          << ". Expected: " << target);

  // Initialise the removal list.
  std::vector<PubKey> removeDSNodePubkeys;

  unsigned int removeResult = DirectoryService::DetermineByzantineNodesCore(
      NUM_OF_ELECTED, removeDSNodePubkeys, STARTING_BLOCK, NUM_OF_FINAL_BLOCK,
      PERFORMANCE_THRESHOLD, NUM_OF_REMOVED, dsComm, dsMemberPerformance);

  // Check the size.
  BOOST_CHECK_MESSAGE(removeResult == target,
                      "removeResult value wrong. Actual: "
                          << removeResult << ". Expected: " << target);
  BOOST_CHECK_MESSAGE(
      removeDSNodePubkeys.size() == target,
      "removeDSNodePubkeys size wrong. Actual: " << removeDSNodePubkeys.size()
                                                 << ". Expected: " << target);

  // Check the keys.
  for (const auto& pubkey : expectedRemoveDSNodePubkeys) {
    BOOST_CHECK_MESSAGE(
        std::find(removeDSNodePubkeys.begin(), removeDSNodePubkeys.end(),
                  pubkey) != removeDSNodePubkeys.end(),
        "Expected pub key " << pubkey << " was not found in the result.");
  }
}

// Test the case when the number of Byzantine nodes is > maxByzantineRemoved.
BOOST_FIXTURE_TEST_CASE(test_MoreThanByzantineNodes, F) {
  INIT_STDOUT_LOGGER();

  // Create the expected removed node list.
  std::vector<PubKey> expectedRemoveDSNodePubkeys;

  // Create the member performance.
  std::map<PubKey, uint32_t> dsMemberPerformance;
  unsigned int count = 0;
  unsigned int target = NUM_OF_REMOVED + 5;
  for (const auto& member : dsComm) {
    if (count < target) {
      dsMemberPerformance[member.first] = 0;
      if (count < NUM_OF_REMOVED) {
        expectedRemoveDSNodePubkeys.emplace_back(member.first);
      }
    } else {
      dsMemberPerformance[member.first] = threshold + 1;
    }
    ++count;
  }

  // Check the expected list.
  BOOST_CHECK_MESSAGE(expectedRemoveDSNodePubkeys.size() == NUM_OF_REMOVED,
                      "expectedRemoveDSNodePubkeys size wrong. Actual: "
                          << expectedRemoveDSNodePubkeys.size()
                          << ". Expected: " << NUM_OF_REMOVED);

  // Initialise the removal list.
  std::vector<PubKey> removeDSNodePubkeys;

  unsigned int removeResult = DirectoryService::DetermineByzantineNodesCore(
      NUM_OF_ELECTED, removeDSNodePubkeys, STARTING_BLOCK, NUM_OF_FINAL_BLOCK,
      PERFORMANCE_THRESHOLD, NUM_OF_REMOVED, dsComm, dsMemberPerformance);

  // Check the size.
  BOOST_CHECK_MESSAGE(removeResult == NUM_OF_REMOVED,
                      "removeResult value wrong. Actual: "
                          << removeResult << ". Expected: " << NUM_OF_REMOVED);
  BOOST_CHECK_MESSAGE(removeDSNodePubkeys.size() == NUM_OF_REMOVED,
                      "removeDSNodePubkeys size wrong. Actual: "
                          << removeDSNodePubkeys.size()
                          << ". Expected: " << NUM_OF_REMOVED);

  // Check the keys.
  for (const auto& pubkey : expectedRemoveDSNodePubkeys) {
    BOOST_CHECK_MESSAGE(
        std::find(removeDSNodePubkeys.begin(), removeDSNodePubkeys.end(),
                  pubkey) != removeDSNodePubkeys.end(),
        "Expected pub key " << pubkey << " was not found in the result.");
  }
}

BOOST_AUTO_TEST_SUITE_END()
