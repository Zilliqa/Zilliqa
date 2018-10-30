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

#include "testLibFunctions.h"

using namespace std;
using namespace boost::multiprecision;

template <typename T>
T randomIntInRng(T n, T m) {
  return std::uniform_int_distribution<T>{n, m}(rng);
}

uint8_t distUint8() {
  return randomIntInRng<uint8_t>(std::numeric_limits<uint8_t>::min(),
                                 std::numeric_limits<uint8_t>::max());
}
uint16_t distUint16() {
  return randomIntInRng<uint16_t>(std::numeric_limits<uint16_t>::min(),
                                  std::numeric_limits<uint16_t>::max());
}
uint32_t distUint32() {
  return randomIntInRng<uint32_t>(std::numeric_limits<uint32_t>::min(),
                                  std::numeric_limits<uint32_t>::max());
}
uint8_t dist1to99() { return randomIntInRng<uint8_t>((uint8_t)1, (uint8_t)99); }

PubKey GenerateRandomPubKey() { return PubKey(PrivKey()); }

Peer GenerateRandomPeer() {
  uint128_t ip_address = distUint32();
  uint32_t listen_port_host = distUint32();
  return Peer(ip_address, listen_port_host);
}

DSBlockHeader GenerateRandomDSBlockHeader() {
  uint8_t dsDifficulty = distUint8();
  uint8_t difficulty = distUint8();
  BlockHash prevHash;
  PubKey leaderPubKey = GenerateRandomPubKey();
  uint64_t blockNum = distUint32();
  uint256_t timestamp = distUint32();
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
  uint8_t type = distUint8();
  uint32_t version = distUint32();
  uint32_t shardId = distUint32();
  uint256_t gasLimit = distUint32();
  uint256_t gasUsed = distUint32();
  uint256_t rewards = distUint32();
  BlockHash prevHash;
  uint64_t blockNum = distUint32();
  uint256_t timestamp = distUint32();
  TxnHash txRootHash;
  uint32_t numTxs = dist1to99();
  PubKey minerPubKey = GenerateRandomPubKey();
  uint64_t dsBlockNum = distUint32();
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
  uint8_t type = distUint8();
  uint32_t version = distUint32();
  uint256_t gasLimit = distUint32();
  uint256_t gasUsed = distUint32();
  uint256_t rewards = distUint32();
  BlockHash prevHash;
  uint64_t blockNum = distUint32();
  uint256_t timestamp = distUint32();
  TxnHash txRootHash;
  StateHash stateRootHash;
  StateHash deltaRootHash;
  StateHash stateDeltaHash;
  TxnHash tranReceiptRootHash;
  uint32_t numTxs = dist1to99();
  uint32_t numMicroBlockHashes = dist1to99();
  PubKey minerPubKey = GenerateRandomPubKey();
  uint64_t dsBlockNum = distUint32();
  BlockHash dsBlockHeader;
  CommitteeHash committeeHash;

  return TxBlockHeader(type, version, gasLimit, gasUsed, rewards, prevHash,
                       blockNum, timestamp, txRootHash, stateRootHash,
                       deltaRootHash, stateDeltaHash, tranReceiptRootHash,
                       numTxs, numMicroBlockHashes, minerPubKey, dsBlockNum,
                       dsBlockHeader, committeeHash);
}

VCBlockHeader GenerateRandomVCBlockHeader() {
  uint64_t vieWChangeDSEpochNo = distUint32();
  uint64_t viewChangeEpochNo = distUint32();
  unsigned char viewChangeState = distUint8();
  uint32_t expectedCandidateLeaderIndex = distUint32();
  Peer candidateLeaderNetworkInfo = GenerateRandomPeer();
  PubKey candidateLeaderPubKey = GenerateRandomPubKey();
  uint32_t vcCounter = distUint32();
  uint256_t timestamp = distUint32();
  CommitteeHash committeeHash;

  return VCBlockHeader(vieWChangeDSEpochNo, viewChangeEpochNo, viewChangeState,
                       expectedCandidateLeaderIndex, candidateLeaderNetworkInfo,
                       candidateLeaderPubKey, vcCounter, timestamp,
                       committeeHash);
}

FallbackBlockHeader GenerateRandomFallbackBlockHeader() {
  uint64_t fallbackDSEpochNo = distUint32();
  uint64_t fallbackEpochNo = distUint32();
  unsigned char fallbackState = distUint8();
  StateHash stateRootHash;
  uint32_t leaderConsensusId = distUint32();
  Peer leaderNetworkInfo = GenerateRandomPeer();
  PubKey leaderPubKey = GenerateRandomPubKey();
  uint32_t shardId = distUint32();
  uint256_t timestamp = distUint32();
  CommitteeHash committeeHash;

  return FallbackBlockHeader(fallbackDSEpochNo, fallbackEpochNo, fallbackState,
                             stateRootHash, leaderConsensusId,
                             leaderNetworkInfo, leaderPubKey, shardId,
                             timestamp, committeeHash);
}

CoSignatures GenerateRandomCoSignatures() { return CoSignatures(dist1to99()); }
