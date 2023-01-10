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
#include "libData/BlockData/BlockHeader/Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

void VCBlockToProtobuf(const VCBlock& vcBlock,
                       ZilliqaMessage::ProtoVCBlock& protoVCBlock) {
  // Serialize header

  ZilliqaMessage::ProtoVCBlock::VCBlockHeader* protoHeader =
      protoVCBlock.mutable_header();

  const VCBlockHeader& header = vcBlock.GetHeader();

  io::VCBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoVCBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(vcBlock, *protoBlockBase);
}

bool SetVCBlock(zbytes& dst, const unsigned int offset,
                const VCBlock& vcBlock) {
  ZilliqaMessage::ProtoVCBlock result;

  VCBlockToProtobuf(vcBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool CheckRequiredFieldsProtoVCBlock(
    const ZilliqaMessage::ProtoVCBlock& /*protoVCBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoVCBlock.has_header() && protoVCBlock.has_blockbase();
#endif
  return true;
}

bool ProtobufToVCBlock(const ZilliqaMessage::ProtoVCBlock& protoVCBlock,
                       VCBlock& vcBlock) {
  if (!CheckRequiredFieldsProtoVCBlock(protoVCBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoVCBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoHeader =
      protoVCBlock.header();

  VCBlockHeader header;

  if (!io::ProtobufToVCBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToVCBlockHeader failed");
    return false;
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoVCBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  vcBlock = VCBlock(header, std::move(coSigs), timestamp);
  return true;
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

  return ProtobufToVCBlock(result, vcBlock);
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

bool VCBlock::operator<(const VCBlock& block) const {
  return block.m_header > m_header;
}

bool VCBlock::operator>(const VCBlock& block) const { return block < *this; }

std::ostream& operator<<(std::ostream& os, const VCBlock& t) {
  const BlockBase& blockBase(t);

  os << "<VCBlock>" << std::endl << blockBase << std::endl << t.GetHeader();
  return os;
}
