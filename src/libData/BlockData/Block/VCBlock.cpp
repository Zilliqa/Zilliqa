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

#include "VCBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
VCBlock::VCBlock() {}

// To-do: handle exceptions. Will be deprecated.
VCBlock::VCBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "Error. We failed to initialize VCBlock.");
  }
}

VCBlock::VCBlock(const VCBlockHeader& header, CoSignatures&& cosigs)
    : m_header(header) {
  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

bool VCBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetVCBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetVCBlock failed.");
    return false;
  }

  return true;
}

bool VCBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetVCBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlock failed.");
    return false;
  }

  return true;
}

const VCBlockHeader& VCBlock::GetHeader() const { return m_header; }

bool VCBlock::operator==(const VCBlock& block) const {
  return (m_header == block.m_header);
}

bool VCBlock::operator<(const VCBlock& block) const {
  return block.m_header > m_header;
}

bool VCBlock::operator>(const VCBlock& block) const { return block < *this; }
