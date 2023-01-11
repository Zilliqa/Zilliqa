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

#include "TxBlock.h"
#include "Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

bool SetTxBlock(zbytes& dst, const unsigned int offset,
                const TxBlock& txBlock) {
  ZilliqaMessage::ProtoTxBlock result;

  io::TxBlockToProtobuf(txBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <std::ranges::contiguous_range RangeT>
bool GetTxBlock(RangeT&& src, unsigned int offset, TxBlock& txBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoTxBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed");
    return false;
  }

  return io::ProtobufToTxBlock(result, txBlock);
}

}  // namespace

bool TxBlock::Serialize(zbytes& dst, unsigned int offset) const {
  if (!SetTxBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxBlock failed.");
    return false;
  }

  return true;
}

bool TxBlock::Deserialize(const zbytes& src, unsigned int offset) {
  return GetTxBlock(src, offset, *this);
}

bool TxBlock::Deserialize(const string& src, unsigned int offset) {
  return GetTxBlock(src, offset, *this);
}

bool TxBlock::operator==(const TxBlock& block) const {
  return ((m_header == block.m_header) && (m_mbInfos == block.m_mbInfos));
}

std::ostream& operator<<(std::ostream& os, const MicroBlockInfo& t) {
  os << "<MicroBlockInfo>" << std::endl
     << " t.m_microBlockHash = " << t.m_microBlockHash << std::endl
     << " t.m_txnRootHash    = " << t.m_txnRootHash << std::endl
     << " t.m_shardId        = " << t.m_shardId;
  return os;
}

std::ostream& operator<<(std::ostream& os, const TxBlock& t) {
  const BlockBase& blockBase(t);

  os << "<TxBlock>" << std::endl
     << blockBase << std::endl
     << t.GetHeader() << std::endl;

  for (const auto& info : t.GetMicroBlockInfos()) {
    os << info << std::endl;
  }

  return os;
}
