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

#include "DSBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
DSBlock::DSBlock() {}

// To-do: handle exceptions. Will be deprecated.
DSBlock::DSBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init DSBlock.");
  }
}

DSBlock::DSBlock(const DSBlockHeader& header, CoSignatures&& cosigs)
    : m_header(header) {
  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

bool DSBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetDSBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlock failed.");
    return false;
  }

  return true;
}

bool DSBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetDSBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlock failed.");
    return false;
  }

  return true;
}

const DSBlockHeader& DSBlock::GetHeader() const { return m_header; }

bool DSBlock::operator==(const DSBlock& block) const {
  return (m_header == block.m_header);
}

bool DSBlock::operator<(const DSBlock& block) const {
  return block.m_header > m_header;
}

bool DSBlock::operator>(const DSBlock& block) const { return block < *this; }
