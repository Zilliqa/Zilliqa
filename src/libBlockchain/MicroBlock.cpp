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

#include "MicroBlock.h"
#include "Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

bool SetMicroBlock(zbytes& dst, const unsigned int offset,
                   const MicroBlock& microBlock) {
  ZilliqaMessage::ProtoMicroBlock result;

  io::MicroBlockToProtobuf(microBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <std::ranges::contiguous_range RangeT>
bool GetMicroBlock(RangeT&& src, unsigned int offset, MicroBlock& microBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoMicroBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed");
    return false;
  }

  if (!io::ProtobufToMicroBlock(result, microBlock)) return false;

  if (microBlock.GetHeader().GetNumTxs() != microBlock.GetTranHashes().size()) {
    LOG_GENERAL(WARNING, "Header txn count ("
                             << microBlock.GetHeader().GetNumTxs()
                             << ") != txn hash count ("
                             << microBlock.GetTranHashes().size() << ")");
    return false;
  }

  return true;
}

}  // namespace

bool MicroBlock::Serialize(zbytes& dst, unsigned int offset) const {
  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Header txn count (" << m_header.GetNumTxs()
                                              << ") != txn hash count ("
                                              << m_tranHashes.size() << ")");
    return false;
  }

  if (!SetMicroBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlock failed.");
    return false;
  }

  return true;
}

bool MicroBlock::Deserialize(const zbytes& src, unsigned int offset) {
  return GetMicroBlock(src, offset, *this);
}

bool MicroBlock::Deserialize(const string& src, unsigned int offset) {
  return GetMicroBlock(src, offset, *this);
}

bool MicroBlock::operator==(const MicroBlock& block) const {
  return ((m_header == block.m_header) && (m_tranHashes == block.m_tranHashes));
}

std::ostream& operator<<(std::ostream& os, const MicroBlock& t) {
  const BlockBase& blockBase(t);

  os << "<MicroBlock>" << std::endl << blockBase << std::endl << t.GetHeader();
  return os;
}
