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
#include "libData/BlockData/BlockHeader/Serialization.h"
#include "libMessage/MessengerCommon.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

void MbInfoToProtobuf(const MicroBlockInfo& mbInfo,
                      ZilliqaMessage::ProtoMbInfo& ProtoMbInfo) {
  ProtoMbInfo.set_mbhash(mbInfo.m_microBlockHash.data(),
                         mbInfo.m_microBlockHash.size);
  ProtoMbInfo.set_txroot(mbInfo.m_txnRootHash.data(),
                         mbInfo.m_txnRootHash.size);
  ProtoMbInfo.set_shardid(mbInfo.m_shardId);
}

void TxBlockToProtobuf(const TxBlock& txBlock,
                       ZilliqaMessage::ProtoTxBlock& protoTxBlock) {
  // Serialize header

  ZilliqaMessage::ProtoTxBlock::TxBlockHeader* protoHeader =
      protoTxBlock.mutable_header();

  const TxBlockHeader& header = txBlock.GetHeader();

  io::TxBlockHeaderToProtobuf(header, *protoHeader);

  for (const auto& mbInfo : txBlock.GetMicroBlockInfos()) {
    auto protoMbInfo = protoTxBlock.add_mbinfos();
    MbInfoToProtobuf(mbInfo, *protoMbInfo);
  }

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoTxBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(txBlock, *protoBlockBase);
}

bool SetTxBlock(zbytes& dst, const unsigned int offset,
                const TxBlock& txBlock) {
  ZilliqaMessage::ProtoTxBlock result;

  TxBlockToProtobuf(txBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool CheckRequiredFieldsProtoTxBlock(
    const ZilliqaMessage::ProtoTxBlock& /*protoTxBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member mbinfos
  return protoTxBlock.has_header() && protoTxBlock.has_blockbase();
#endif
  return true;
}

bool CheckRequiredFieldsProtoMbInfo(
    const ZilliqaMessage::ProtoMbInfo& /*protoMbInfo*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoMbInfo.has_mbhash() && protoMbInfo.has_txroot() &&
         protoMbInfo.has_shardid();
#endif
  return true;
}

bool ProtobufToMbInfo(const ZilliqaMessage::ProtoMbInfo& ProtoMbInfo,
                      MicroBlockInfo& mbInfo) {
  if (!CheckRequiredFieldsProtoMbInfo(ProtoMbInfo)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMbInfo failed");
    return false;
  }

  copy(ProtoMbInfo.mbhash().begin(),
       ProtoMbInfo.mbhash().begin() +
           min((unsigned int)ProtoMbInfo.mbhash().size(),
               (unsigned int)mbInfo.m_microBlockHash.size),
       mbInfo.m_microBlockHash.asArray().begin());
  copy(ProtoMbInfo.txroot().begin(),
       ProtoMbInfo.txroot().begin() +
           min((unsigned int)ProtoMbInfo.txroot().size(),
               (unsigned int)mbInfo.m_txnRootHash.size),
       mbInfo.m_txnRootHash.asArray().begin());
  mbInfo.m_shardId = ProtoMbInfo.shardid();

  return true;
}

bool ProtobufToTxBlock(const ZilliqaMessage::ProtoTxBlock& protoTxBlock,
                       TxBlock& txBlock) {
  if (!CheckRequiredFieldsProtoTxBlock(protoTxBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTxBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoHeader =
      protoTxBlock.header();

  TxBlockHeader header;

  if (!io::ProtobufToTxBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToTxBlockHeader failed");
    return false;
  }

  // Deserialize body
  vector<MicroBlockInfo> mbInfos;

  for (const auto& protoMbInfo : protoTxBlock.mbinfos()) {
    MicroBlockInfo mbInfo;
    if (!ProtobufToMbInfo(protoMbInfo, mbInfo)) {
      return false;
    }
    mbInfos.emplace_back(mbInfo);
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoTxBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  txBlock = TxBlock(header, mbInfos, std::move(coSigs), timestamp);
  return true;
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

  return ProtobufToTxBlock(result, txBlock);
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
