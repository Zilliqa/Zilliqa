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

VCBlock::VCBlock(const VCBlockHeader& header, CoSignatures&& cosigs)
    : BlockBase{header.GetMyHash(), std::move(cosigs)}, m_header(header) {}

bool VCBlock::Serialize(zbytes& dst, unsigned int offset) const {
  if (!Messenger::SetVCBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetVCBlock failed.");
    return false;
  }

  return true;
}

bool VCBlock::Deserialize(const zbytes& src, unsigned int offset) {
  if (!Messenger::GetVCBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlock failed.");
    return false;
  }

  return true;
}

bool VCBlock::Deserialize(const string& src, unsigned int offset) {
  if (!Messenger::GetVCBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlock failed.");
    return false;
  }

  return true;
}

bool VCBlock::operator==(const VCBlock& block) const {
  return (m_header == block.m_header);
}

bool VCBlock::operator<(const VCBlock& block) const {
  return block.m_header > m_header;
}

bool VCBlock::operator>(const VCBlock& block) const { return block < *this; }


std::ostream& operator<<(std::ostream& os, const VCBlock& t) {
  const BlockBase& blockBase(t);

  os << "<VCBlock>" << std::endl << blockBase << std::endl << t.m_header;
  return os;
}
