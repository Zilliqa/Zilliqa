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

#include <array>
#include <string>
#include <vector>

#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libTestUtils/TestUtils.h"

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

void PrintShard(const DequeOfShard& shards) {
  for (const auto& shard : shards) {
    LOG_GENERAL(INFO, "Shard:")
    for (const auto& node : shard) {
      LOG_GENERAL(INFO, "  Node: " << get<SHARD_NODE_PEER>(node) << " "
                                   << get<SHARD_NODE_PUBKEY>(node));
    }
  }
}

void PrintDSCommittee(const DequeOfNode& dsCommittee) {
  LOG_GENERAL(INFO, "DS Committee:")
  for (const auto& dsnode : dsCommittee) {
    LOG_GENERAL(INFO, "  Node: " << dsnode.second << " " << dsnode.first);
  }
}

void PrintCoinbase(const DiagnosticDataCoinbase& entry) {
  LOG_GENERAL(INFO, "Coinbase:")
  LOG_GENERAL(INFO, "  nodeCount: " << entry.nodeCount);
  LOG_GENERAL(INFO, "  sigCount: " << entry.sigCount);
  LOG_GENERAL(INFO, "  lookupCount: " << entry.lookupCount);
  LOG_GENERAL(INFO, "  totalReward: " << entry.totalReward);
  LOG_GENERAL(INFO, "  baseReward: " << entry.baseReward);
  LOG_GENERAL(INFO, "  baseRewardEach: " << entry.baseRewardEach);
  LOG_GENERAL(INFO, "  lookupReward: " << entry.lookupReward);
  LOG_GENERAL(INFO, "  rewardEachLookup: " << entry.rewardEachLookup);
  LOG_GENERAL(INFO, "  nodeReward: " << entry.nodeReward);
  LOG_GENERAL(INFO, "  rewardEach: " << entry.rewardEach);
  LOG_GENERAL(INFO, "  balanceLeft: " << entry.balanceLeft);
  LOG_GENERAL(INFO, "  luckyDrawWinnerKey: " << entry.luckyDrawWinnerKey);
  LOG_GENERAL(INFO, "  luckyDrawWinnerAddr: " << entry.luckyDrawWinnerAddr);
}

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testDiagnosticDataNodes) {
  INIT_STDOUT_LOGGER();

  // Clear the database first
  BlockStorage::GetBlockStorage().ResetDB(
      BlockStorage::DBTYPE::DIAGNOSTIC_NODES);

  vector<uint64_t> histDSBlockNum;
  vector<DequeOfShard> histShards;
  vector<DequeOfNode> histDSCommittee;

  const unsigned int NUM_ENTRIES = 15;

  // Test writing and looking up all entries
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    histDSBlockNum.emplace_back(i);
    histShards.emplace_back(TestUtils::GenerateDequeueOfShard(2));
    histDSCommittee.emplace_back(TestUtils::GenerateRandomDSCommittee(3));

    LOG_GENERAL(
        INFO, "Storing diagnostic data for DS block " << histDSBlockNum.back());
    PrintShard(histShards.back());
    PrintDSCommittee(histDSCommittee.back());

    BOOST_CHECK(BlockStorage::GetBlockStorage().PutDiagnosticDataNodes(
        histDSBlockNum.back(), histShards.back(), histDSCommittee.back()));
  }

  // Look-up by block number
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    DequeOfShard shardsDeserialized;
    DequeOfNode dsCommitteeDeserialized;

    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataNodes(
        histDSBlockNum.at(i), shardsDeserialized, dsCommitteeDeserialized));

    BOOST_CHECK(shardsDeserialized == histShards.at(i));
    BOOST_CHECK(dsCommitteeDeserialized == histDSCommittee.at(i));
  }

  // Look-up by dumping all contents
  map<uint64_t, DiagnosticDataNodes> diagnosticDataMap;
  BlockStorage::GetBlockStorage().GetDiagnosticDataNodes(diagnosticDataMap);
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    BOOST_CHECK(diagnosticDataMap.count(i) == 1);
    BOOST_CHECK(diagnosticDataMap[i].shards == histShards.at(i));
    BOOST_CHECK(diagnosticDataMap[i].dsCommittee == histDSCommittee.at(i));
  }

  // Test deletion of entries
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    // First, check the entry is still there
    DequeOfShard shardsDeserialized;
    DequeOfNode dsCommitteeDeserialized;

    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataNodes(
        histDSBlockNum.at(i), shardsDeserialized, dsCommitteeDeserialized));

    BOOST_CHECK(shardsDeserialized == histShards.at(i));
    BOOST_CHECK(dsCommitteeDeserialized == histDSCommittee.at(i));

    // Check the db size
    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataNodesCount() ==
                (NUM_ENTRIES - i));

    // Then, delete the entry
    BOOST_CHECK(BlockStorage::GetBlockStorage().DeleteDiagnosticDataNodes(
        histDSBlockNum.at(i)));

    // Check that the entry has been deleted
    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataNodes(
                    histDSBlockNum.at(i), shardsDeserialized,
                    dsCommitteeDeserialized) == false);
    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataNodesCount() ==
                (NUM_ENTRIES - i - 1));
  }
}

BOOST_AUTO_TEST_CASE(testDiagnosticDataCoinbase) {
  INIT_STDOUT_LOGGER();

  // Clear the database first
  BlockStorage::GetBlockStorage().ResetDB(
      BlockStorage::DBTYPE::DIAGNOSTIC_COINBASE);

  vector<uint64_t> histDSBlockNum;
  vector<DiagnosticDataCoinbase> histEntries;

  const unsigned int NUM_ENTRIES = 15;

  // Test writing and looking up all entries
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    DiagnosticDataCoinbase entry = {TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint32(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::DistUint128(),
                                    TestUtils::GenerateRandomPubKey(),
                                    Address()};
    histDSBlockNum.emplace_back(i);
    histEntries.emplace_back(entry);

    LOG_GENERAL(
        INFO, "Storing diagnostic data for DS block " << histDSBlockNum.back());
    PrintCoinbase(histEntries.back());

    BOOST_CHECK(BlockStorage::GetBlockStorage().PutDiagnosticDataCoinbase(
        histDSBlockNum.back(), histEntries.back()));
  }

  // Look-up by block number
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    DiagnosticDataCoinbase entryDeserialized;

    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbase(
        histDSBlockNum.at(i), entryDeserialized));

    BOOST_CHECK(entryDeserialized == histEntries.at(i));
  }

  // Look-up by dumping all contents
  map<uint64_t, DiagnosticDataCoinbase> diagnosticDataMap;
  BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbase(diagnosticDataMap);
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    BOOST_CHECK(diagnosticDataMap.count(i) == 1);
    BOOST_CHECK(diagnosticDataMap[i] == histEntries.at(i));
  }

  // Test deletion of entries
  for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
    // First, check the entry is still there
    DiagnosticDataCoinbase entryDeserialized;

    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbase(
        histDSBlockNum.at(i), entryDeserialized));

    BOOST_CHECK(entryDeserialized == histEntries.at(i));

    // Check the db size
    BOOST_CHECK(
        BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbaseCount() ==
        (NUM_ENTRIES - i));

    // Then, delete the entry
    BOOST_CHECK(BlockStorage::GetBlockStorage().DeleteDiagnosticDataCoinbase(
        histDSBlockNum.at(i)));

    // Check that the entry has been deleted
    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbase(
                    histDSBlockNum.at(i), entryDeserialized) == false);
    BOOST_CHECK(
        BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbaseCount() ==
        (NUM_ENTRIES - i - 1));
  }
}

BOOST_AUTO_TEST_SUITE_END()
