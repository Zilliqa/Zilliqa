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

#include "FallbackBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

FallbackBlock::FallbackBlock() {}

FallbackBlock::FallbackBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init FallbackBlock");
  }
}

FallbackBlock::FallbackBlock(const FallbackBlockHeader& header,
                             CoSignatures&& cosigs)
    : m_header(header) {
  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

bool FallbackBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetFallbackBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetFallbackBlock failed.");
    return false;
  }

  return true;
}

bool FallbackBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetFallbackBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetFallbackBlock failed.");
    return false;
  }

  return true;
}

const FallbackBlockHeader& FallbackBlock::GetHeader() const { return m_header; }

bool FallbackBlock::operator==(const FallbackBlock& block) const {
  return (m_header == block.m_header);
}

bool FallbackBlock::operator<(const FallbackBlock& block) const {
  return block.m_header > m_header;
}

bool FallbackBlock::operator>(const FallbackBlock& block) const {
  return block < *this;
}
