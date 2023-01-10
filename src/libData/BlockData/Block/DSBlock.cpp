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
#include "libData/BlockData/BlockHeader/Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

void DSBlockToProtobuf(const DSBlock& dsBlock,
                       ZilliqaMessage::ProtoDSBlock& protoDSBlock) {
  // Serialize header

  ZilliqaMessage::ProtoDSBlock::DSBlockHeader* protoHeader =
      protoDSBlock.mutable_header();

  const DSBlockHeader& header = dsBlock.GetHeader();

  io::DSBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoDSBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(dsBlock, *protoBlockBase);
}

bool SetDSBlock(zbytes& dst, const unsigned int offset,
                const DSBlock& dsBlock) {
  ZilliqaMessage::ProtoDSBlock result;

  DSBlockToProtobuf(dsBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool CheckRequiredFieldsProtoDSBlock(
    const ZilliqaMessage::ProtoDSBlock& /*protoDSBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoDSBlock.has_header() && protoDSBlock.has_blockbase();
#endif
  return true;
}

bool ProtobufToDSBlock(const ZilliqaMessage::ProtoDSBlock& protoDSBlock,
                       DSBlock& dsBlock) {
  // Deserialize header

  if (!CheckRequiredFieldsProtoDSBlock(protoDSBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSBlock failed");
    return false;
  }

  const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoHeader =
      protoDSBlock.header();

  DSBlockHeader header;
  if (!io::ProtobufToDSBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToDSBlockHeader failed");
    return false;
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoDSBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  dsBlock = DSBlock{header, std::move(coSigs), timestamp};
  return true;
}

template <std::ranges::contiguous_range RangeT>
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

  return ProtobufToDSBlock(result, dsBlock);
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
