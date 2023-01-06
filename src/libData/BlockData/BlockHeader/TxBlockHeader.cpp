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

using namespace std;
using namespace boost::multiprecision;

TxBlockHeader::TxBlockHeader(uint64_t gasLimit, uint64_t gasUsed,
                             const uint128_t& rewards, uint64_t blockNum,
                             const TxBlockHashSet& blockHashSet,
                             uint32_t numTxs, const PubKey& minerPubKey,
                             uint64_t dsBlockNum, uint32_t version,
                             const CommitteeHash& committeeHash,
                             const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_gasLimit(gasLimit),
      m_gasUsed(gasUsed),
      m_rewards(rewards),
      m_blockNum(blockNum),
      m_hashset(blockHashSet),
      m_numTxs(numTxs),
      m_minerPubKey(minerPubKey),
      m_dsBlockNum(dsBlockNum) {}

bool TxBlockHeader::Serialize(zbytes& dst, unsigned int offset) const {
  if (!Messenger::SetTxBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxBlockHeader failed.");
    return false;
  }

  return true;
}

bool TxBlockHeader::Deserialize(const zbytes& src, unsigned int offset) {
  if (!Messenger::GetTxBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxBlockHeader failed.");
    return false;
  }

  return true;
}

bool TxBlockHeader::Deserialize(const string& src, unsigned int offset) {
  if (!Messenger::GetTxBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxBlockHeader failed.");
    return false;
  }

  return true;
}

bool TxBlockHeader::operator==(const TxBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_gasLimit, m_gasUsed, m_rewards, m_blockNum, m_hashset,
                   m_numTxs, m_minerPubKey, m_dsBlockNum) ==
          std::tie(header.m_gasLimit, header.m_gasUsed, header.m_rewards,
                   header.m_blockNum, header.m_hashset, header.m_numTxs,
                   header.m_minerPubKey, header.m_dsBlockNum));
}

#if 0
bool TxBlockHeader::operator<(const TxBlockHeader& header) const {
  return m_blockNum < header.m_blockNum;
}

bool TxBlockHeader::operator>(const TxBlockHeader& header) const {
  return header < *this;
}
#endif

std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<TxBlockHeader>" << std::endl
     << " GasLimit    = " << t.GetGasLimit() << std::endl
     << " GasUsed     = " << t.GetGasUsed() << std::endl
     << " Rewards     = " << t.GetRewards() << std::endl
     << " BlockNum    = " << t.GetBlockNum() << std::endl
     << " NumTxs      = " << t.GetNumTxs() << std::endl
     << " MinerPubKey = " << t.GetMinerPubKey() << std::endl
     << " DSBlockNum  = " << t.GetDSBlockNum() << std::endl
     << t.m_hashset;
  return os;
}
