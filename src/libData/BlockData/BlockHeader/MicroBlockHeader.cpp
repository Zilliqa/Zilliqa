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

#include "MicroBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

MicroBlockHeader::MicroBlockHeader()
    : m_shardId(0),
      m_gasLimit(0),
      m_gasUsed(0),
      m_rewards(0),
      m_epochNum((uint64_t)-1),
      m_hashset(),
      m_numTxs(0),
      m_minerPubKey(),
      m_dsBlockNum(INIT_BLOCK_NUMBER) {}

MicroBlockHeader::MicroBlockHeader(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init MicroBlockHeader.");
  }
}

MicroBlockHeader::MicroBlockHeader(
    uint32_t shardId, const uint64_t& gasLimit, const uint64_t& gasUsed,
    const uint128_t& rewards, const uint64_t& epochNum,
    const MicroBlockHashSet& hashset, uint32_t numTxs,
    const PubKey& minerPubKey, const uint64_t& dsBlockNum,
    const uint32_t version, const CommitteeHash& committeeHash,
    const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_shardId(shardId),
      m_gasLimit(gasLimit),
      m_gasUsed(gasUsed),
      m_rewards(rewards),
      m_epochNum(epochNum),
      m_hashset(hashset),
      m_numTxs(numTxs),
      m_minerPubKey(minerPubKey),
      m_dsBlockNum(dsBlockNum) {}

bool MicroBlockHeader::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetMicroBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlockHeader failed.");
    return false;
  }

  return true;
}

bool MicroBlockHeader::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetMicroBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetMicroBlockHeader failed.");
    return false;
  }

  return true;
}

const uint32_t& MicroBlockHeader::GetShardId() const { return m_shardId; }

const uint64_t& MicroBlockHeader::GetGasLimit() const { return m_gasLimit; }

const uint64_t& MicroBlockHeader::GetGasUsed() const { return m_gasUsed; }

const uint128_t& MicroBlockHeader::GetRewards() const { return m_rewards; }

const uint64_t& MicroBlockHeader::GetEpochNum() const { return m_epochNum; }

const uint32_t& MicroBlockHeader::GetNumTxs() const { return m_numTxs; }

const PubKey& MicroBlockHeader::GetMinerPubKey() const { return m_minerPubKey; }

const uint64_t& MicroBlockHeader::GetDSBlockNum() const { return m_dsBlockNum; }

const TxnHash& MicroBlockHeader::GetTxRootHash() const {
  return m_hashset.m_txRootHash;
}

const StateHash& MicroBlockHeader::GetStateDeltaHash() const {
  return m_hashset.m_stateDeltaHash;
}

const TxnHash& MicroBlockHeader::GetTranReceiptHash() const {
  return m_hashset.m_tranReceiptHash;
}

const MicroBlockHashSet& MicroBlockHeader::GetHashes() const {
  return m_hashset;
}

bool MicroBlockHeader::operator==(const MicroBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_shardId, m_gasLimit, m_gasUsed, m_rewards, m_epochNum,
                   m_hashset, m_numTxs, m_minerPubKey, m_dsBlockNum) ==
          std::tie(header.m_shardId, header.m_gasLimit, header.m_gasUsed,
                   header.m_rewards, header.m_epochNum, header.m_hashset,
                   header.m_numTxs, header.m_minerPubKey, header.m_dsBlockNum));
}

bool MicroBlockHeader::operator<(const MicroBlockHeader& header) const {
  // To compare, first they must be of identical epochno
  return (std::tie(m_version, m_prevHash, m_epochNum, m_dsBlockNum) ==
          std::tie(header.m_version, header.m_prevHash, header.m_epochNum,
                   header.m_dsBlockNum)) &&
         (m_shardId < header.m_shardId);
}

bool MicroBlockHeader::operator>(const MicroBlockHeader& header) const {
  return header < *this;
}
