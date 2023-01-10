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
#include "libData/BlockData/BlockHeader/Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

void MicroBlockToProtobuf(const MicroBlock& microBlock,
                          ZilliqaMessage::ProtoMicroBlock& protoMicroBlock) {
  // Serialize header

  ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader* protoHeader =
      protoMicroBlock.mutable_header();

  const MicroBlockHeader& header = microBlock.GetHeader();

  io::MicroBlockHeaderToProtobuf(header, *protoHeader);

  // Serialize body

  for (const auto& hash : microBlock.GetTranHashes()) {
    protoMicroBlock.add_tranhashes(hash.data(), hash.size);
  }

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoMicroBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(microBlock, *protoBlockBase);
}

bool SetMicroBlock(zbytes& dst, const unsigned int offset,
                   const MicroBlock& microBlock) {
  ZilliqaMessage::ProtoMicroBlock result;

  MicroBlockToProtobuf(microBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool CheckRequiredFieldsProtoMicroBlock(
    const ZilliqaMessage::ProtoMicroBlock& /*protoMicroBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member tranhashes
  return protoMicroBlock.has_header() && protoMicroBlock.has_blockbase();
#endif
  return true;
}

bool ProtobufToMicroBlock(
    const ZilliqaMessage::ProtoMicroBlock& protoMicroBlock,
    MicroBlock& microBlock) {
  if (!CheckRequiredFieldsProtoMicroBlock(protoMicroBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMicroBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader& protoHeader =
      protoMicroBlock.header();

  MicroBlockHeader header;

  if (!io::ProtobufToMicroBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToMicroBlockHeader failed");
    return false;
  }

  // Deserialize body

  vector<TxnHash> tranHashes;
  for (const auto& hash : protoMicroBlock.tranhashes()) {
    tranHashes.emplace_back();
    unsigned int size =
        min((unsigned int)hash.size(), (unsigned int)tranHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         tranHashes.back().asArray().begin());
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoMicroBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  microBlock = MicroBlock{header, tranHashes, std::move(coSigs), timestamp};
  return true;
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

  if (!ProtobufToMicroBlock(result, microBlock)) return false;

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

bool MicroBlock::operator<(const MicroBlock& block) const {
  return std::tie(block.m_header, block.m_tranHashes) >
         std::tie(m_header, m_tranHashes);
}

bool MicroBlock::operator>(const MicroBlock& block) const {
  return block < *this;
}

std::ostream& operator<<(std::ostream& os, const MicroBlock& t) {
  const BlockBase& blockBase(t);

  os << "<MicroBlock>" << std::endl << blockBase << std::endl << t.GetHeader();
  return os;
}
