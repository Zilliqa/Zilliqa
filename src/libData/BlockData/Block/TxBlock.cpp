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

#include <utility>

#include "TxBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

bool TxBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTxBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxBlock failed.");
    return false;
  }

  return true;
}

bool TxBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetTxBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxBlock failed.");
    return false;
  }

  return true;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
TxBlock::TxBlock() {}

TxBlock::TxBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init TxBlock.");
  }
}

TxBlock::TxBlock(const TxBlockHeader& header,
                 const vector<MicroBlockInfo>& mbInfos, CoSignatures&& cosigs)
    : m_header(header), m_mbInfos(mbInfos) {
  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

const TxBlockHeader& TxBlock::GetHeader() const { return m_header; }

const std::vector<MicroBlockInfo>& TxBlock::GetMicroBlockInfos() const {
  return m_mbInfos;
}

bool TxBlock::operator==(const TxBlock& block) const {
  return ((m_header == block.m_header) && (m_mbInfos == block.m_mbInfos));
}

bool TxBlock::operator<(const TxBlock& block) const {
  return std::tie(block.m_header, block.m_mbInfos) >
         std::tie(m_header, m_mbInfos);
}

bool TxBlock::operator>(const TxBlock& block) const { return block < *this; }
