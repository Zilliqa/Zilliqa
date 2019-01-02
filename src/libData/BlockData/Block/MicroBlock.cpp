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

#include "MicroBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

bool MicroBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Header txn count (" << m_header.GetNumTxs()
                                              << ") != txn hash count ("
                                              << m_tranHashes.size() << ")");
    return false;
  }

  if (!Messenger::SetMicroBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlock failed.");
    return false;
  }

  return true;
}

bool MicroBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetMicroBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetMicroBlock failed.");
    return false;
  }

  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Header txn count (" << m_header.GetNumTxs()
                                              << ") != txn hash count ("
                                              << m_tranHashes.size() << ")");
    return false;
  }

  return true;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
MicroBlock::MicroBlock() {}

MicroBlock::MicroBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init MicroBlock.");
  }
}

MicroBlock::MicroBlock(const MicroBlockHeader& header,
                       const vector<TxnHash>& tranHashes, CoSignatures&& cosigs)
    : m_header(header), m_tranHashes(tranHashes) {
  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Num of Txns get from header "
                             << m_header.GetNumTxs()
                             << " is not equal to the size of m_tranHashes "
                             << m_tranHashes.size());
  }

  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

const MicroBlockHeader& MicroBlock::GetHeader() const { return m_header; }

const vector<TxnHash>& MicroBlock::GetTranHashes() const {
  return m_tranHashes;
}

bool MicroBlock::operator==(const MicroBlock& block) const {
  return ((m_header == block.m_header) && (m_tranHashes == block.m_tranHashes));
}

bool MicroBlock::operator<(const MicroBlock& block) const {
  return std::tie(block.m_header, block.m_tranHashes) >
         std::tie(m_header, m_tranHashes);
}

bool MicroBlock::operator>(const MicroBlock& block) const {
  return block < *this;
}
