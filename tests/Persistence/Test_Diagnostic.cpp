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

#include <array>
#include <string>
#include <vector>

#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libTestUtils/TestUtils.h"

#define BOOST_TEST_MODULE persistencetest
#include <boost/test/included/unit_test.hpp>

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

void PrintDSCommittee(const DequeOfDSNode& dsCommittee) {
  LOG_GENERAL(INFO, "DS Committee:")
  for (const auto& dsnode : dsCommittee) {
    LOG_GENERAL(INFO, "  Node: " << dsnode.second << " " << dsnode.first);
  }
}

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testDiagnostic) {
  INIT_STDOUT_LOGGER();

  vector<uint64_t> histDSBlockNum;
  vector<DequeOfShard> histShards;
  vector<DequeOfDSNode> histDSCommittee;

  for (unsigned int i = 0; i < 3; i++) {
    histDSBlockNum.emplace_back(TestUtils::DistUint64());
    histShards.emplace_back(TestUtils::GenerateDequeueOfShard(2));
    histDSCommittee.emplace_back(TestUtils::GenerateRandomDSCommittee(3));

    LOG_GENERAL(
        INFO, "Storing diagnostic data for DS block " << histDSBlockNum.back());
    PrintShard(histShards.back());
    PrintDSCommittee(histDSCommittee.back());

    BOOST_CHECK(BlockStorage::GetBlockStorage().PutDiagnosticData(
        histDSBlockNum.back(), histShards.back(), histDSCommittee.back()));
  }

  for (unsigned int i = 0; i < 3; i++) {
    DequeOfShard shardsDeserialized;
    DequeOfDSNode dsCommitteeDeserialized;

    BOOST_CHECK(BlockStorage::GetBlockStorage().GetDiagnosticData(
        histDSBlockNum.at(i), shardsDeserialized, dsCommitteeDeserialized));

    BOOST_CHECK(shardsDeserialized == histShards.at(i));
    BOOST_CHECK(dsCommitteeDeserialized == histDSCommittee.at(i));
  }
}

BOOST_AUTO_TEST_SUITE_END()
