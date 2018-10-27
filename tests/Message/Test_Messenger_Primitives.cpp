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
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

std::mt19937 rng;
std::uniform_int_distribution<std::mt19937::result_type> dist1to99(1, 99);
std::uniform_int_distribution<std::mt19937::result_type> distUint8(
    std::numeric_limits<uint8_t>::min(), std::numeric_limits<uint8_t>::max());
std::uniform_int_distribution<std::mt19937::result_type> distUint16(
    std::numeric_limits<uint16_t>::min(), std::numeric_limits<uint16_t>::max());
std::uniform_int_distribution<std::mt19937::result_type> distUint32(
    std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
std::uniform_int_distribution<std::mt19937::result_type> distUint64(
    std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());

PubKey GenerateRandomPubKey() { return PubKey(PrivKey()); }

Peer GenerateRandomPeer() {
  uint128_t ip_address = distUint64(rng);
  uint32_t listen_port_host = distUint32(rng);
  return Peer(ip_address, listen_port_host);
}

DSBlockHeader GenerateRandomDSBlockHeader() {
  uint8_t dsDifficulty = distUint8(rng);
  uint8_t difficulty = distUint8(rng);
  BlockHash prevHash;
  PubKey leaderPubKey = GenerateRandomPubKey();
  uint64_t blockNum = distUint64(rng);
  uint256_t timestamp = distUint64(rng);
  SWInfo swInfo;
  map<PubKey, Peer> powDSWinners;
  DSBlockHashSet hash;
  CommitteeHash committeeHash;
  for (unsigned int i = 0; i < 3; i++) {
    powDSWinners.emplace(GenerateRandomPubKey(), GenerateRandomPeer());
  }

  return DSBlockHeader(dsDifficulty, difficulty, prevHash, leaderPubKey,
                       blockNum, timestamp, swInfo, powDSWinners, hash,
                       committeeHash);
}

MicroBlockHeader GenerateRandomMicroBlockHeader() {
  uint8_t type = distUint8(rng);
  uint32_t version = distUint32(rng);
  uint32_t shardId = distUint32(rng);
  uint256_t gasLimit = distUint64(rng);
  uint256_t gasUsed = distUint64(rng);
  uint256_t rewards = distUint64(rng);
  BlockHash prevHash;
  uint64_t blockNum = distUint64(rng);
  uint256_t timestamp = distUint64(rng);
  TxnHash txRootHash;
  uint32_t numTxs = dist1to99(rng);
  PubKey minerPubKey = GenerateRandomPubKey();
  uint64_t dsBlockNum = distUint64(rng);
  BlockHash dsBlockHash;
  StateHash stateDeltaHash;
  TxnHash tranReceiptHash;
  CommitteeHash committeeHash;

  return MicroBlockHeader(type, version, shardId, gasLimit, gasUsed, rewards,
                          prevHash, blockNum, timestamp, txRootHash, numTxs,
                          minerPubKey, dsBlockNum, dsBlockHash, stateDeltaHash,
                          tranReceiptHash, committeeHash);
}

TxBlockHeader GenerateRandomTxBlockHeader() {
  uint8_t type = distUint8(rng);
  uint32_t version = distUint32(rng);
  uint256_t gasLimit = distUint64(rng);
  uint256_t gasUsed = distUint64(rng);
  uint256_t rewards = distUint64(rng);
  BlockHash prevHash;
  uint64_t blockNum = distUint64(rng);
  uint256_t timestamp = distUint64(rng);
  TxnHash txRootHash;
  StateHash stateRootHash;
  StateHash deltaRootHash;
  StateHash stateDeltaHash;
  TxnHash tranReceiptRootHash;
  uint32_t numTxs = dist1to99(rng);
  uint32_t numMicroBlockHashes = dist1to99(rng);
  PubKey minerPubKey = GenerateRandomPubKey();
  uint64_t dsBlockNum = distUint64(rng);
  BlockHash dsBlockHeader;
  CommitteeHash committeeHash;

  return TxBlockHeader(type, version, gasLimit, gasUsed, rewards, prevHash,
                       blockNum, timestamp, txRootHash, stateRootHash,
                       deltaRootHash, stateDeltaHash, tranReceiptRootHash,
                       numTxs, numMicroBlockHashes, minerPubKey, dsBlockNum,
                       dsBlockHeader, committeeHash);
}

VCBlockHeader GenerateRandomVCBlockHeader() {
  uint64_t vieWChangeDSEpochNo = distUint64(rng);
  uint64_t viewChangeEpochNo = distUint64(rng);
  unsigned char viewChangeState = distUint8(rng);
  uint32_t expectedCandidateLeaderIndex = distUint32(rng);
  Peer candidateLeaderNetworkInfo = GenerateRandomPeer();
  PubKey candidateLeaderPubKey = GenerateRandomPubKey();
  uint32_t vcCounter = distUint32(rng);
  uint256_t timestamp = distUint64(rng);
  CommitteeHash committeeHash;

  return VCBlockHeader(vieWChangeDSEpochNo, viewChangeEpochNo, viewChangeState,
                       expectedCandidateLeaderIndex, candidateLeaderNetworkInfo,
                       candidateLeaderPubKey, vcCounter, timestamp,
                       committeeHash);
}

FallbackBlockHeader GenerateRandomFallbackBlockHeader() {
  uint64_t fallbackDSEpochNo = distUint64(rng);
  uint64_t fallbackEpochNo = distUint64(rng);
  unsigned char fallbackState = distUint8(rng);
  StateHash stateRootHash;
  uint32_t leaderConsensusId = distUint32(rng);
  Peer leaderNetworkInfo = GenerateRandomPeer();
  PubKey leaderPubKey = GenerateRandomPubKey();
  uint32_t shardId = distUint32(rng);
  uint256_t timestamp = distUint64(rng);
  CommitteeHash committeeHash;

  return FallbackBlockHeader(fallbackDSEpochNo, fallbackEpochNo, fallbackState,
                             stateRootHash, leaderConsensusId,
                             leaderNetworkInfo, leaderPubKey, shardId,
                             timestamp, committeeHash);
}

CoSignatures GenerateRandomCoSignatures() {
  return CoSignatures(distUint8(rng));
}

BOOST_AUTO_TEST_SUITE(messenger_primitives_test)

BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  rng.seed(std::random_device()());
}

BOOST_AUTO_TEST_CASE(test_GetDSCommitteeHash) {
  deque<pair<PubKey, Peer>> dsCommittee;
  CommitteeHash dst;

  for (unsigned int i = 0, count = dist1to99(rng); i < count; i++) {
    dsCommittee.emplace_back(GenerateRandomPubKey(), GenerateRandomPeer());
  }

  BOOST_CHECK(Messenger::GetDSCommitteeHash(dsCommittee, dst));
}

BOOST_AUTO_TEST_CASE(test_GetShardHash) {
  Shard shard;
  CommitteeHash dst;

  for (unsigned int i = 0, count = dist1to99(rng); i < count; i++) {
    shard.emplace_back(GenerateRandomPubKey(), GenerateRandomPeer(),
                       distUint16(rng));
  }

  BOOST_CHECK(Messenger::GetShardHash(shard, dst));
}

BOOST_AUTO_TEST_CASE(test_GetShardingStructureHash) {
  DequeOfShard shards;
  ShardingHash dst;

  for (unsigned int i = 0, count = dist1to99(rng); i < count; i++) {
    shards.emplace_back();
    for (unsigned int j = 0, countj = dist1to99(rng); j < countj; j++) {
      shards.back().emplace_back(GenerateRandomPubKey(), GenerateRandomPeer(),
                                 distUint16(rng));
    }
  }

  BOOST_CHECK(Messenger::GetShardingStructureHash(shards, dst));
}

BOOST_AUTO_TEST_CASE(test_GetTxSharingAssignmentsHash) {
  vector<Peer> dsReceivers;
  vector<vector<Peer>> shardReceivers;
  vector<vector<Peer>> shardSenders;
  TxSharingHash dst;

  for (unsigned int i = 0, count = dist1to99(rng); i < count; i++) {
    dsReceivers.emplace_back();
  }

  for (unsigned int i = 0, count = dist1to99(rng); i < count; i++) {
    shardReceivers.emplace_back();
    shardSenders.emplace_back();
    for (unsigned int j = 0, countj = dist1to99(rng); j < countj; j++) {
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

  DSBlockHeader dsBlockHeader = GenerateRandomDSBlockHeader();

  BOOST_CHECK(Messenger::SetDSBlockHeader(dst, offset, dsBlockHeader));

  DSBlockHeader dsBlockHeaderDeserialized;

  BOOST_CHECK(
      Messenger::GetDSBlockHeader(dst, offset, dsBlockHeaderDeserialized));

  BOOST_CHECK(dsBlockHeader == dsBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetDSBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  DSBlock dsBlock(GenerateRandomDSBlockHeader(), GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetDSBlock(dst, offset, dsBlock));

  DSBlock dsBlockDeserialized;

  BOOST_CHECK(Messenger::GetDSBlock(dst, offset, dsBlockDeserialized));

  BOOST_CHECK(dsBlock == dsBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetMicroBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  MicroBlockHeader microBlockHeader = GenerateRandomMicroBlockHeader();

  BOOST_CHECK(Messenger::SetMicroBlockHeader(dst, offset, microBlockHeader));

  MicroBlockHeader microBlockHeaderDeserialized;

  BOOST_CHECK(Messenger::GetMicroBlockHeader(dst, offset,
                                             microBlockHeaderDeserialized));

  BOOST_CHECK(microBlockHeader == microBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetMicroBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  MicroBlockHeader microBlockHeader = GenerateRandomMicroBlockHeader();
  vector<TxnHash> tranHashes;

  for (unsigned int i = 0; i < microBlockHeader.GetNumTxs(); i++) {
    tranHashes.emplace_back();
  }

  MicroBlock microBlock(microBlockHeader, tranHashes,
                        GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetMicroBlock(dst, offset, microBlock));

  MicroBlock microBlockDeserialized;

  BOOST_CHECK(Messenger::GetMicroBlock(dst, offset, microBlockDeserialized));

  BOOST_CHECK(microBlock == microBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetTxBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  TxBlockHeader txBlockHeader = GenerateRandomTxBlockHeader();

  BOOST_CHECK(Messenger::SetTxBlockHeader(dst, offset, txBlockHeader));

  TxBlockHeader txBlockHeaderDeserialized;

  BOOST_CHECK(
      Messenger::GetTxBlockHeader(dst, offset, txBlockHeaderDeserialized));

  BOOST_CHECK(txBlockHeader == txBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetTxBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;

  TxBlockHeader txBlockHeader = GenerateRandomTxBlockHeader();
  vector<bool> isMicroBlockEmpty(txBlockHeader.GetNumMicroBlockHashes());
  vector<MicroBlockHashSet> microBlockHashes;
  vector<uint32_t> shardIds(txBlockHeader.GetNumMicroBlockHashes());

  TxBlock txBlock(txBlockHeader, isMicroBlockEmpty, microBlockHashes, shardIds,
                  GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetTxBlock(dst, offset, txBlock));

  TxBlock txBlockDeserialized;

  BOOST_CHECK(Messenger::GetTxBlock(dst, offset, txBlockDeserialized));

  BOOST_CHECK(txBlock == txBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetVCBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  VCBlockHeader vcBlockHeader = GenerateRandomVCBlockHeader();

  BOOST_CHECK(Messenger::SetVCBlockHeader(dst, offset, vcBlockHeader));

  VCBlockHeader vcBlockHeaderDeserialized;

  BOOST_CHECK(
      Messenger::GetVCBlockHeader(dst, offset, vcBlockHeaderDeserialized));

  BOOST_CHECK(vcBlockHeader == vcBlockHeaderDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetVCBlock) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  VCBlock vcBlock(GenerateRandomVCBlockHeader(), GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetVCBlock(dst, offset, vcBlock));

  VCBlock vcBlockDeserialized;

  BOOST_CHECK(Messenger::GetVCBlock(dst, offset, vcBlockDeserialized));

  BOOST_CHECK(vcBlock == vcBlockDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetFallbackBlockHeader) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  FallbackBlockHeader fallbackBlockHeader = GenerateRandomFallbackBlockHeader();

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
  FallbackBlock fallbackBlock(GenerateRandomFallbackBlockHeader(),
                              GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetFallbackBlock(dst, offset, fallbackBlock));

  FallbackBlock fallbackBlockDeserialized;

  BOOST_CHECK(
      Messenger::GetFallbackBlock(dst, offset, fallbackBlockDeserialized));

  BOOST_CHECK(fallbackBlock == fallbackBlockDeserialized);
}

BOOST_AUTO_TEST_SUITE_END()