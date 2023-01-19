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

#include "DSBlock.h"
#include "Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

bool SetDSBlock(zbytes& dst, const unsigned int offset,
                const DSBlock& dsBlock) {
  ZilliqaMessage::ProtoDSBlock result;

  io::DSBlockToProtobuf(dsBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

#ifdef __APPLE__
template <typename RangeT>
#else
template <std::ranges::contiguous_range RangeT>
#endif
bool GetDSBlock(RangeT&& src, unsigned int offset, DSBlock& dsBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoDSBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed");
    return false;
  }

  return io::ProtobufToDSBlock(result, dsBlock);
}

}  // namespace

bool DSBlock::Serialize(zbytes& dst, unsigned int offset) const {
  if (!SetDSBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlock failed.");
    return false;
  }

  return true;
}

bool DSBlock::Deserialize(const zbytes& src, unsigned int offset) {
  return GetDSBlock(src, offset, *this);
}

bool DSBlock::Deserialize(const string& src, unsigned int offset) {
  return GetDSBlock(src, offset, *this);
}

bool DSBlock::operator==(const DSBlock& block) const {
  return (m_header == block.m_header);
}

std::ostream& operator<<(std::ostream& os, const DSBlock& t) {
  const BlockBase& blockBase(t);

  os << "<DSBlock>" << std::endl << blockBase << std::endl << t.GetHeader();
  return os;
}
