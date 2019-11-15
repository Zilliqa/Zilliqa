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

#define BOOST_TEST_MODULE savedsperformance
#define BOOST_TEST_DYN_LINK
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <boost/test/unit_test.hpp>

#define COMMITTEE_SIZE 20
#define NUM_OF_ELECTED 5
#define NUM_OF_REMOVED 2
#define LOCALHOST 0x7F000001
#define BASE_PORT 2600
#define EPOCH_NUM 1
#define NUM_OF_FINAL_BLOCK 100
#define STARTING_BLOCK 200
#define FINALBLOCK_REWARD -1

using namespace std;

BOOST_AUTO_TEST_SUITE(savedsperformance)

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

    // Generate a sample shard.
    for (int i = 0; i < COMMITTEE_SIZE; ++i) {
      PairOfKey kp = Schnorr::GenKeyPair();
      PubKey pk = kp.second;
      Peer peer = Peer(LOCALHOST + 1, BASE_PORT + i);
      PairOfNode entry = std::make_pair(pk, peer);
      shard.emplace_back(entry);
    }
  }

  ~F() { BOOST_TEST_MESSAGE("teardown fixture"); }

  PairOfKey selfKeyPair;
  PubKey selfPubKey;
  DequeOfNode dsComm;
  DequeOfNode shard;
};

// Test that no previous performance data is carried over.

BOOST_FIXTURE_TEST_CASE(test_CleanSave, F) {
  INIT_STDOUT_LOGGER();

  // Create an empty coinbase rewards.
  std::map<uint64_t, std::map<int32_t, std::vector<PubKey>>> coinbaseRewardees;

  // Create some previous data in the member performance.
  std::map<PubKey, uint32_t> dsMemberPerformance;
  for (const auto& member : dsComm) {
    dsMemberPerformance[member.first] = 10;
  }

  DirectoryService::SaveDSPerformanceCore(
      coinbaseRewardees, dsMemberPerformance, dsComm, EPOCH_NUM,
      NUM_OF_FINAL_BLOCK, FINALBLOCK_REWARD);

  // Check the size.
  BOOST_CHECK_MESSAGE(dsMemberPerformance.size() == COMMITTEE_SIZE,
                      "Expected DS Performance size wrong. Actual: "
                          << dsMemberPerformance.size()
                          << ". Expected: " << COMMITTEE_SIZE);

  // Check the result.
  for (const auto& member : dsComm) {
    BOOST_CHECK_MESSAGE(dsMemberPerformance[member.first] == 0,
                        "Pub Key " << member.first
                                   << " is not cleared. Expected: 0. Actual: "
                                   << dsMemberPerformance[member.first]);
  }
}

// Test the legitimate case.

BOOST_FIXTURE_TEST_CASE(test_LegitimateCase, F) {
  INIT_STDOUT_LOGGER();

  // Create the expected member performance.
  std::map<PubKey, uint32_t> expectedDSMemberPerformance;
  for (const auto& member : dsComm) {
    expectedDSMemberPerformance[member.first] = 0;
  }

  // Populate the coinbase rewards.
  std::srand(std::time(nullptr));
  std::map<uint64_t, std::map<int32_t, std::vector<PubKey>>> coinbaseRewardees;
  // NUM_OF_FINAL_BLOCK - 1 because coinbase is distributed on vacuous epoch.
  for (int i = 0; i < (NUM_OF_FINAL_BLOCK - 1); ++i) {
    unsigned int epoch = i + STARTING_BLOCK;
    std::map<int32_t, std::vector<PubKey>> epochMap;

    // Add reward for shard -1 (DS)
    std::vector<PubKey> dsRewards;
    for (const auto& member : dsComm) {
      // Perform this twice because consensus requires 2 co-sigs.
      for (int times = 0; times < 2; ++times) {
        // Randomly decide if the co-sig was performed on a 50% probability.
        if (std::rand() % 2 == 0) {
          dsRewards.emplace_back(member.first);
          ++expectedDSMemberPerformance[member.first];
        }
      }
    }
    epochMap[FINALBLOCK_REWARD] = dsRewards;

    // Add reward for shard 0
    std::vector<PubKey> shardRewards;
    for (const auto& member : shard) {
      // Perform this twice because consensus requires 2 co-sigs.
      for (int times = 0; times < 2; ++times) {
        // Randomly decide if the co-sig was performed on a 50% probability.
        if (std::rand() % 2 == 0) {
          shardRewards.emplace_back(member.first);
        }
      }
    }
    epochMap[0] = shardRewards;

    // Add all the shards
    coinbaseRewardees[epoch] = epochMap;
  }

  // Check the source/expected values are sane.
  BOOST_CHECK_MESSAGE(coinbaseRewardees.size() == (NUM_OF_FINAL_BLOCK - 1),
                      "coinbaseRewardees size wrong. Actual: "
                          << coinbaseRewardees.size()
                          << ". Expected: " << (NUM_OF_FINAL_BLOCK - 1));

  BOOST_CHECK_MESSAGE(expectedDSMemberPerformance.size() == COMMITTEE_SIZE,
                      "expectedDSMemberPerformance size wrong. Actual: "
                          << expectedDSMemberPerformance.size()
                          << ". Expected: " << COMMITTEE_SIZE);

  // Create the member performance.
  std::map<PubKey, uint32_t> dsMemberPerformance;

  DirectoryService::SaveDSPerformanceCore(
      coinbaseRewardees, dsMemberPerformance, dsComm, EPOCH_NUM,
      NUM_OF_FINAL_BLOCK, FINALBLOCK_REWARD);

  // Check the size.
  BOOST_CHECK_MESSAGE(dsMemberPerformance.size() == COMMITTEE_SIZE,
                      "Expected DS Performance size wrong. Actual: "
                          << dsMemberPerformance.size()
                          << ". Expected: " << COMMITTEE_SIZE);

  // Check the result.
  for (const auto& member : dsComm) {
    BOOST_CHECK_MESSAGE(
        dsMemberPerformance[member.first] ==
            expectedDSMemberPerformance[member.first],
        "Pub Key " << member.first << " performance does not match. Actual: "
                   << dsMemberPerformance[member.first] << ". Expected: "
                   << expectedDSMemberPerformance[member.first]);
  }
}

BOOST_AUTO_TEST_SUITE_END()
