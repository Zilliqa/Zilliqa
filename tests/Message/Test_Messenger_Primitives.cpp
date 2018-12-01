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

#include <limits>
#include <random>
#include "libMessage/Messenger.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(messenger_primitives_test)

BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  TestUtils::Initialize();
}

BOOST_AUTO_TEST_CASE(test_GetDSCommitteeHash) {
  deque<pair<PubKey, Peer>> dsCommittee;
  CommitteeHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    dsCommittee.emplace_back(TestUtils::GenerateRandomPubKey(),
                             TestUtils::GenerateRandomPeer());
  }

  BOOST_CHECK(Messenger::GetDSCommitteeHash(dsCommittee, dst));
}

BOOST_AUTO_TEST_CASE(test_GetShardHash) {
  Shard shard;
  CommitteeHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    shard.emplace_back(TestUtils::GenerateRandomPubKey(),
                       TestUtils::GenerateRandomPeer(),
                       TestUtils::DistUint16());
  }

  BOOST_CHECK(Messenger::GetShardHash(shard, dst));
}

BOOST_AUTO_TEST_CASE(test_GetShardingStructureHash) {
  DequeOfShard shards;
  ShardingHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    shards.emplace_back();
    for (unsigned int j = 0, countj = TestUtils::Dist1to99(); j < countj; j++) {
      shards.back().emplace_back(TestUtils::GenerateRandomPubKey(),
                                 TestUtils::GenerateRandomPeer(),
                                 TestUtils::DistUint16());
    }
  }

  BOOST_CHECK(Messenger::GetShardingStructureHash(shards, dst));
}

BOOST_AUTO_TEST_CASE(test_GetTxSharingAssignmentsHash) {
  vector<Peer> dsReceivers;
  vector<vector<Peer>> shardReceivers;
  vector<vector<Peer>> shardSenders;
  TxSharingHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    dsReceivers.emplace_back();
  }

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    shardReceivers.emplace_back();
    shardSenders.emplace_back();
    for (unsigned int j = 0, countj = TestUtils::Dist1to99(); j < countj; j++) {
      shardReceivers.back().emplace_back();
      shardSenders.back().emplace_back();
    }
  }

  BOOST_CHECK(Messenger::GetTxSharingAssignmentsHash(
      dsReceivers, shardReceivers, shardSenders, dst));
}

BOOST_AUTO_TEST_CASE(test_SetAndGetDSBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  DSBlockHeader dsBlockHeader = TestUtils::GenerateRandomDSBlockHeader();

  BOOST_CHECK(Messenger::SetDSBlockHeader(dst, offset, dsBlockHeader));

  DSBlockHeader dsBlockHeaderDeserialized;

  BOOST_CHECK(
      Messenger::GetDSBlockHeader(dst, offset, dsBlockHeaderDeserialized));

  BOOST_CHECK(dsBlockHeader == dsBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetDSBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  DSBlock dsBlock(TestUtils::GenerateRandomDSBlockHeader(),
                  TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetDSBlock(dst, offset, dsBlock));

  DSBlock dsBlockDeserialized;

  BOOST_CHECK(Messenger::GetDSBlock(dst, offset, dsBlockDeserialized));

  BOOST_CHECK(dsBlock == dsBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetMicroBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  MicroBlockHeader microBlockHeader =
      TestUtils::GenerateRandomMicroBlockHeader();

  BOOST_CHECK(Messenger::SetMicroBlockHeader(dst, offset, microBlockHeader));

  MicroBlockHeader microBlockHeaderDeserialized;

  BOOST_CHECK(Messenger::GetMicroBlockHeader(dst, offset,
                                             microBlockHeaderDeserialized));

  BOOST_CHECK(microBlockHeader == microBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetMicroBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  MicroBlockHeader microBlockHeader =
      TestUtils::GenerateRandomMicroBlockHeader();
  vector<TxnHash> tranHashes;

  for (unsigned int i = 0; i < microBlockHeader.GetNumTxs(); i++) {
    tranHashes.emplace_back();
  }

  MicroBlock microBlock(microBlockHeader, tranHashes,
                        TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetMicroBlock(dst, offset, microBlock));

  MicroBlock microBlockDeserialized;

  BOOST_CHECK(Messenger::GetMicroBlock(dst, offset, microBlockDeserialized));

  BOOST_CHECK(microBlock == microBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetTxBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  TxBlockHeader txBlockHeader = TestUtils::GenerateRandomTxBlockHeader();

  BOOST_CHECK(Messenger::SetTxBlockHeader(dst, offset, txBlockHeader));

  TxBlockHeader txBlockHeaderDeserialized;

  BOOST_CHECK(
      Messenger::GetTxBlockHeader(dst, offset, txBlockHeaderDeserialized));

  BOOST_CHECK(txBlockHeader == txBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetTxBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  TxBlockHeader txBlockHeader = TestUtils::GenerateRandomTxBlockHeader();
  vector<MicroBlockInfo> microBlockInfo(0);

  TxBlock txBlock(txBlockHeader, microBlockInfo,
                  TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetTxBlock(dst, offset, txBlock));

  TxBlock txBlockDeserialized;

  BOOST_CHECK(Messenger::GetTxBlock(dst, offset, txBlockDeserialized));

  BOOST_CHECK(txBlock == txBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetVCBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  VCBlockHeader vcBlockHeader = TestUtils::GenerateRandomVCBlockHeader();

  BOOST_CHECK(Messenger::SetVCBlockHeader(dst, offset, vcBlockHeader));

  VCBlockHeader vcBlockHeaderDeserialized;

  BOOST_CHECK(
      Messenger::GetVCBlockHeader(dst, offset, vcBlockHeaderDeserialized));

  BOOST_CHECK(vcBlockHeader == vcBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetVCBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  VCBlock vcBlock(TestUtils::GenerateRandomVCBlockHeader(),
                  TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetVCBlock(dst, offset, vcBlock));

  VCBlock vcBlockDeserialized;

  BOOST_CHECK(Messenger::GetVCBlock(dst, offset, vcBlockDeserialized));

  BOOST_CHECK(vcBlock == vcBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetFallbackBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  FallbackBlockHeader fallbackBlockHeader =
      TestUtils::GenerateRandomFallbackBlockHeader();

  BOOST_CHECK(
      Messenger::SetFallbackBlockHeader(dst, offset, fallbackBlockHeader));

  FallbackBlockHeader fallbackBlockHeaderDeserialized;

  BOOST_CHECK(Messenger::GetFallbackBlockHeader(
      dst, offset, fallbackBlockHeaderDeserialized));

  BOOST_CHECK(fallbackBlockHeader == fallbackBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetFallbackBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  FallbackBlock fallbackBlock(TestUtils::GenerateRandomFallbackBlockHeader(),
                              TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetFallbackBlock(dst, offset, fallbackBlock));

  FallbackBlock fallbackBlockDeserialized;

  BOOST_CHECK(
      Messenger::GetFallbackBlock(dst, offset, fallbackBlockDeserialized));

  BOOST_CHECK(fallbackBlock == fallbackBlockDeserialized);
}

BOOST_AUTO_TEST_SUITE_END()
