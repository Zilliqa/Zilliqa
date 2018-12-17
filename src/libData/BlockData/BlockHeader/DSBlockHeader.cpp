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

#include "DSBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

DSBlockHeader::DSBlockHeader()
    : m_dsDifficulty(0),
      m_difficulty(0),
      m_prevHash(),
      m_leaderPubKey(),
      m_blockNum(INIT_BLOCK_NUMBER),
      m_epochNum((uint64_t)-1),
      m_gasPrice(0),
      m_swInfo(),
      m_PoWDSWinners(),
      m_hashset() {}

DSBlockHeader::DSBlockHeader(const vector<unsigned char>& src,
                             unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init DSBlockHeader.");
  }
}

DSBlockHeader::DSBlockHeader(const uint8_t dsDifficulty,
                             const uint8_t difficulty,
                             const BlockHash& prevHash,
                             const PubKey& leaderPubKey,
                             const uint64_t& blockNum, const uint64_t& epochNum,
                             const uint128_t& gasPrice, const SWInfo& swInfo,
                             const map<PubKey, Peer>& powDSWinners,
                             const DSBlockHashSet& hashset,
                             const CommitteeHash& committeeHash)
    : BlockHeaderBase(committeeHash),
      m_dsDifficulty(dsDifficulty),
      m_difficulty(difficulty),
      m_prevHash(prevHash),
      m_leaderPubKey(leaderPubKey),
      m_blockNum(blockNum),
      m_epochNum(epochNum),
      m_gasPrice(gasPrice),
      m_swInfo(swInfo),
      m_PoWDSWinners(powDSWinners),
      m_hashset(hashset) {}

bool DSBlockHeader::Serialize(vector<unsigned char>& dst,
                              unsigned int offset) const {
  if (!Messenger::SetDSBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return false;
  }

#if 1  // clark
  LOG_GENERAL(INFO, "dst = " << string(dst.begin(), dst.end()));
#endif

  return true;
}

BlockHash DSBlockHeader::GetHashForRandom() const {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  std::vector<unsigned char> vec;

  if (!Messenger::SetDSBlockHeader(vec, 0, *this, true)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return BlockHash();
  }

  sha2.Update(vec);
  const std::vector<unsigned char>& resVec = sha2.Finalize();
  BlockHash blockHash;
  std::copy(resVec.begin(), resVec.end(), blockHash.asArray().begin());
  return blockHash;
}

bool DSBlockHeader::Deserialize(const vector<unsigned char>& src,
                                unsigned int offset) {
  if (!Messenger::GetDSBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlockHeader failed.");
    return false;
  }

  return true;
}

const uint8_t& DSBlockHeader::GetDSDifficulty() const { return m_dsDifficulty; }

const uint8_t& DSBlockHeader::GetDifficulty() const { return m_difficulty; }

const BlockHash& DSBlockHeader::GetPrevHash() const { return m_prevHash; }

const PubKey& DSBlockHeader::GetLeaderPubKey() const { return m_leaderPubKey; }

const uint64_t& DSBlockHeader::GetBlockNum() const { return m_blockNum; }

const uint64_t& DSBlockHeader::GetEpochNum() const { return m_epochNum; }

const uint128_t& DSBlockHeader::GetGasPrice() const { return m_gasPrice; }

const SWInfo& DSBlockHeader::GetSWInfo() const { return m_swInfo; }

const map<PubKey, Peer>& DSBlockHeader::GetDSPoWWinners() const {
  return m_PoWDSWinners;
}

const ShardingHash& DSBlockHeader::GetShardingHash() const {
  return m_hashset.m_shardingHash;
}

const array<unsigned char, RESERVED_FIELD_SIZE>&
DSBlockHeader::GetHashSetReservedField() const {
  return m_hashset.m_reservedField;
}

bool DSBlockHeader::operator==(const DSBlockHeader& header) const {
  return tie(m_dsDifficulty, m_difficulty, m_prevHash, m_leaderPubKey,
             m_blockNum, m_gasPrice, m_swInfo, m_PoWDSWinners) ==
         tie(header.m_dsDifficulty, header.m_difficulty, header.m_prevHash,
             header.m_leaderPubKey, header.m_blockNum, header.m_gasPrice,
             header.m_swInfo, header.m_PoWDSWinners);
}

bool DSBlockHeader::operator<(const DSBlockHeader& header) const {
  return m_blockNum < header.m_blockNum;
}

bool DSBlockHeader::operator>(const DSBlockHeader& header) const {
  return header < *this;
}
