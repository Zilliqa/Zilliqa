/*
 * Copyright (C) 2022 Zilliqa
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

#include "Serialization.h"
#include "DSBlock.h"
#include "MicroBlock.h"
#include "TxBlock.h"
#include "VCBlock.h"
#include "libData/BlockData/BlockHeader/Serialization.h"

namespace io {

namespace {

constexpr bool CheckRequiredFieldsProtoBlockBase(
    const ZilliqaMessage::ProtoBlockBase& /*protoBlockBase*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockBase.has_blockhash() && protoBlockBase.has_cosigs() &&
         protoBlockBase.has_timestamp() &&
         CheckRequiredFieldsProtoBlockBaseCoSignatures(protoBlockBase.cosigs());
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoDSBlock(
    const ZilliqaMessage::ProtoDSBlock& /*protoDSBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoDSBlock.has_header() && protoDSBlock.has_blockbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoMicroBlock(
    const ZilliqaMessage::ProtoMicroBlock& /*protoMicroBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member tranhashes
  return protoMicroBlock.has_header() && protoMicroBlock.has_blockbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoTxBlock(
    const ZilliqaMessage::ProtoTxBlock& /*protoTxBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member mbinfos
  return protoTxBlock.has_header() && protoTxBlock.has_blockbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoMbInfo(
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
           std::min((unsigned int)ProtoMbInfo.mbhash().size(),
                    (unsigned int)mbInfo.m_microBlockHash.size),
       mbInfo.m_microBlockHash.asArray().begin());
  copy(ProtoMbInfo.txroot().begin(),
       ProtoMbInfo.txroot().begin() +
           std::min((unsigned int)ProtoMbInfo.txroot().size(),
                    (unsigned int)mbInfo.m_txnRootHash.size),
       mbInfo.m_txnRootHash.asArray().begin());
  mbInfo.m_shardId = ProtoMbInfo.shardid();

  return true;
}

constexpr bool CheckRequiredFieldsProtoVCBlock(
    const ZilliqaMessage::ProtoVCBlock& /*protoVCBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoVCBlock.has_header() && protoVCBlock.has_blockbase();
#endif
  return true;
}

}  // namespace

void BlockBaseToProtobuf(const BlockBase& base,
                         ZilliqaMessage::ProtoBlockBase& protoBlockBase) {
  // Block hash

  protoBlockBase.set_blockhash(base.GetBlockHash().data(),
                               base.GetBlockHash().size);

  // Timestampo
  protoBlockBase.set_timestamp(base.GetTimestamp());

  // Serialize cosigs

  ZilliqaMessage::ProtoBlockBase::CoSignatures* cosigs =
      protoBlockBase.mutable_cosigs();

  SerializableToProtobufByteArray(base.GetCS1(), *cosigs->mutable_cs1());
  for (const auto& i : base.GetB1()) {
    cosigs->add_b1(i);
  }
  SerializableToProtobufByteArray(base.GetCS2(), *cosigs->mutable_cs2());
  for (const auto& i : base.GetB2()) {
    cosigs->add_b2(i);
  }
}

std::optional<std::tuple<BlockHash, CoSignatures, uint64_t>>
ProtobufToBlockBase(const ZilliqaMessage::ProtoBlockBase& protoBlockBase) {
  if (!CheckRequiredFieldsProtoBlockBase(protoBlockBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockBase failed");
    return std::nullopt;
  }

  // Deserialize cosigs
  CoSignatures cosigs;
  cosigs.m_B1.resize(protoBlockBase.cosigs().b1().size());
  cosigs.m_B2.resize(protoBlockBase.cosigs().b2().size());

  PROTOBUFBYTEARRAYTOSERIALIZABLEOPT(protoBlockBase.cosigs().cs1(),
                                     cosigs.m_CS1);
  copy(protoBlockBase.cosigs().b1().begin(), protoBlockBase.cosigs().b1().end(),
       cosigs.m_B1.begin());
  PROTOBUFBYTEARRAYTOSERIALIZABLEOPT(protoBlockBase.cosigs().cs2(),
                                     cosigs.m_CS2);
  copy(protoBlockBase.cosigs().b2().begin(), protoBlockBase.cosigs().b2().end(),
       cosigs.m_B2.begin());

  // Deserialize the block hash
  BlockHash blockHash;
  if (!CopyWithSizeCheck(protoBlockBase.blockhash(), blockHash.asArray())) {
    return std::nullopt;
  }

  // Deserialize timestamp
  uint64_t timestamp;
  timestamp = protoBlockBase.timestamp();

  return std::make_tuple(std::move(blockHash), std::move(cosigs), timestamp);
}

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

  std::vector<TxnHash> tranHashes;
  for (const auto& hash : protoMicroBlock.tranhashes()) {
    tranHashes.emplace_back();
    unsigned int size = std::min((unsigned int)hash.size(),
                                 (unsigned int)tranHashes.back().size);
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
  std::vector<MicroBlockInfo> mbInfos;

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

void MbInfoToProtobuf(const MicroBlockInfo& mbInfo,
                      ZilliqaMessage::ProtoMbInfo& ProtoMbInfo) {
  ProtoMbInfo.set_mbhash(mbInfo.m_microBlockHash.data(),
                         mbInfo.m_microBlockHash.size);
  ProtoMbInfo.set_txroot(mbInfo.m_txnRootHash.data(),
                         mbInfo.m_txnRootHash.size);
  ProtoMbInfo.set_shardid(mbInfo.m_shardId);
}

}  // namespace io
