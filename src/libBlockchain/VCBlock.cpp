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
#include "Serialization.h"
#include "libBlockchain/Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

bool SetVCBlock(zbytes& dst, const unsigned int offset,
                const VCBlock& vcBlock) {
  ZilliqaMessage::ProtoVCBlock result;

  io::VCBlockToProtobuf(vcBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <std::ranges::contiguous_range RangeT>
bool GetVCBlock(RangeT&& src, unsigned int offset, VCBlock& vcBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoVCBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed");
    return false;
  }

  return io::ProtobufToVCBlock(result, vcBlock);
}

}  // namespace

bool VCBlock::Serialize(zbytes& dst, unsigned int offset) const {
  if (!SetVCBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetVCBlock failed.");
    return false;
  }

  return true;
}

bool VCBlock::Deserialize(const zbytes& src, unsigned int offset) {
  return GetVCBlock(src, offset, *this);
}

bool VCBlock::Deserialize(const string& src, unsigned int offset) {
  return GetVCBlock(src, offset, *this);
}

bool VCBlock::operator==(const VCBlock& block) const {
  return (m_header == block.m_header);
}

std::ostream& operator<<(std::ostream& os, const VCBlock& t) {
  const BlockBase& blockBase(t);

  os << "<VCBlock>" << std::endl << blockBase << std::endl << t.GetHeader();
  return os;
}
