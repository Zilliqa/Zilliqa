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

#include "DSBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

DSBlockHeader::DSBlockHeader()
    : m_dsDifficulty(0),
      m_difficulty(0),
      m_leaderPubKey(),
      m_blockNum(INIT_BLOCK_NUMBER),
      m_epochNum((uint64_t)-1),
      m_gasPrice(0),
      m_swInfo(),
      m_PoWDSWinners(),
      m_hashset() {}

DSBlockHeader::DSBlockHeader(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init DSBlockHeader.");
  }
}

DSBlockHeader::DSBlockHeader(
    const uint8_t dsDifficulty, const uint8_t difficulty,
    const PubKey& leaderPubKey, const uint64_t& blockNum,
    const uint64_t& epochNum, const uint128_t& gasPrice, const SWInfo& swInfo,
    const map<PubKey, Peer>& powDSWinners, const DSBlockHashSet& hashset,
    const uint32_t version, const CommitteeHash& committeeHash,
    const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_dsDifficulty(dsDifficulty),
      m_difficulty(difficulty),
      m_leaderPubKey(leaderPubKey),
      m_blockNum(blockNum),
      m_epochNum(epochNum),
      m_gasPrice(gasPrice),
      m_swInfo(swInfo),
      m_PoWDSWinners(powDSWinners),
      m_hashset(hashset) {}

bool DSBlockHeader::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetDSBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return false;
  }

  return true;
}

BlockHash DSBlockHeader::GetHashForRandom() const {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  bytes vec;

  if (!Messenger::SetDSBlockHeader(vec, 0, *this, true)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return BlockHash();
  }

  sha2.Update(vec);
  const bytes& resVec = sha2.Finalize();
  BlockHash blockHash;
  std::copy(resVec.begin(), resVec.end(), blockHash.asArray().begin());
  return blockHash;
}

bool DSBlockHeader::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetDSBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlockHeader failed.");
    return false;
  }

  return true;
}

const uint8_t& DSBlockHeader::GetDSDifficulty() const { return m_dsDifficulty; }

const uint8_t& DSBlockHeader::GetDifficulty() const { return m_difficulty; }

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
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_dsDifficulty, m_difficulty, m_leaderPubKey, m_blockNum,
                   m_gasPrice, m_swInfo, m_PoWDSWinners) ==
          std::tie(header.m_dsDifficulty, header.m_difficulty,
                   header.m_leaderPubKey, header.m_blockNum, header.m_gasPrice,
                   header.m_swInfo, header.m_PoWDSWinners));
}

bool DSBlockHeader::operator<(const DSBlockHeader& header) const {
  return m_blockNum < header.m_blockNum;
}

bool DSBlockHeader::operator>(const DSBlockHeader& header) const {
  return header < *this;
}
