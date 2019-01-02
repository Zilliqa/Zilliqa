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

#include "TxBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

TxBlockHeader::TxBlockHeader()
    : m_type(0),
      m_version(0),
      m_gasLimit(0),
      m_gasUsed(0),
      m_rewards(0),
      m_prevHash(),
      m_blockNum(INIT_BLOCK_NUMBER),
      m_hashset(),
      m_numTxs(0),
      m_minerPubKey(),
      m_dsBlockNum(INIT_BLOCK_NUMBER) {}

TxBlockHeader::TxBlockHeader(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init TxBlockHeader.");
  }
}

TxBlockHeader::TxBlockHeader(uint8_t type, uint32_t version,
                             const uint64_t& gasLimit, const uint64_t& gasUsed,
                             const uint128_t& rewards,
                             const BlockHash& prevHash,
                             const uint64_t& blockNum,
                             const TxBlockHashSet& blockHashSet,
                             uint32_t numTxs, const PubKey& minerPubKey,
                             const uint64_t& dsBlockNum,
                             const CommitteeHash& committeeHash)
    : BlockHeaderBase(committeeHash),
      m_type(type),
      m_version(version),
      m_gasLimit(gasLimit),
      m_gasUsed(gasUsed),
      m_rewards(rewards),
      m_prevHash(prevHash),
      m_blockNum(blockNum),
      m_hashset(blockHashSet),
      m_numTxs(numTxs),
      m_minerPubKey(minerPubKey),
      m_dsBlockNum(dsBlockNum) {}

bool TxBlockHeader::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTxBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxBlockHeader failed.");
    return false;
  }

  return true;
}

bool TxBlockHeader::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetTxBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxBlockHeader failed.");
    return false;
  }

  return true;
}

const uint8_t& TxBlockHeader::GetType() const { return m_type; }

const uint32_t& TxBlockHeader::GetVersion() const { return m_version; }

const uint64_t& TxBlockHeader::GetGasLimit() const { return m_gasLimit; }

const uint64_t& TxBlockHeader::GetGasUsed() const { return m_gasUsed; }

const uint128_t& TxBlockHeader::GetRewards() const { return m_rewards; }

const BlockHash& TxBlockHeader::GetPrevHash() const { return m_prevHash; }

const uint64_t& TxBlockHeader::GetBlockNum() const { return m_blockNum; }

const StateHash& TxBlockHeader::GetStateRootHash() const {
  return m_hashset.m_stateRootHash;
}

const StateHash& TxBlockHeader::GetStateDeltaHash() const {
  return m_hashset.m_stateDeltaHash;
}

const MBInfoHash& TxBlockHeader::GetMbInfoHash() const {
  return m_hashset.m_mbInfoHash;
}

const uint32_t& TxBlockHeader::GetNumTxs() const { return m_numTxs; }

const PubKey& TxBlockHeader::GetMinerPubKey() const { return m_minerPubKey; }

const uint64_t& TxBlockHeader::GetDSBlockNum() const { return m_dsBlockNum; }

bool TxBlockHeader::operator==(const TxBlockHeader& header) const {
  return std::tie(m_type, m_version, m_gasLimit, m_gasUsed, m_rewards,
                  m_prevHash, m_blockNum, m_hashset, m_numTxs, m_minerPubKey,
                  m_dsBlockNum) ==
         std::tie(header.m_type, header.m_version, header.m_gasLimit,
                  header.m_gasUsed, header.m_rewards, header.m_prevHash,
                  header.m_blockNum, header.m_hashset, header.m_numTxs,
                  header.m_minerPubKey, header.m_dsBlockNum);
}

bool TxBlockHeader::operator<(const TxBlockHeader& header) const {
  return std::tie(header.m_type, header.m_version, header.m_gasLimit,
                  header.m_gasUsed, header.m_rewards, header.m_prevHash,
                  header.m_blockNum, header.m_hashset, header.m_numTxs,
                  header.m_minerPubKey, header.m_dsBlockNum) >
         std::tie(m_type, m_version, m_gasLimit, m_gasUsed, m_rewards,
                  m_prevHash, m_blockNum, m_hashset, m_numTxs, m_minerPubKey,
                  m_dsBlockNum);
}

bool TxBlockHeader::operator>(const TxBlockHeader& header) const {
  return header < *this;
}
