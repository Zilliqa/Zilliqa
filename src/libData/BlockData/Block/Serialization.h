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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_SERIALIZATION_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_SERIALIZATION_H_

#include "BlockBase.h"
#include "libMessage/MessengerCommon.h"

class DSBlock;
class MicroBlock;
class TxBlock;
class VCBlock;
struct MicroBlockInfo;

namespace io {

void BlockBaseToProtobuf(const BlockBase& base,
                         ZilliqaMessage::ProtoBlockBase& protoBlockBase);

std::optional<std::tuple<BlockHash, CoSignatures, uint64_t>>
ProtobufToBlockBase(const ZilliqaMessage::ProtoBlockBase& protoBlockBase);

void DSBlockToProtobuf(const DSBlock& dsBlock,
                       ZilliqaMessage::ProtoDSBlock& protoDSBlock);

bool ProtobufToDSBlock(const ZilliqaMessage::ProtoDSBlock& protoDSBlock,
                       DSBlock& dsBlock);

void MicroBlockToProtobuf(const MicroBlock& microBlock,
                          ZilliqaMessage::ProtoMicroBlock& protoMicroBlock);

bool ProtobufToMicroBlock(
    const ZilliqaMessage::ProtoMicroBlock& protoMicroBlock,
    MicroBlock& microBlock);

void TxBlockToProtobuf(const TxBlock& txBlock,
                       ZilliqaMessage::ProtoTxBlock& protoTxBlock);

bool ProtobufToTxBlock(const ZilliqaMessage::ProtoTxBlock& protoTxBlock,
                       TxBlock& txBlock);

void VCBlockToProtobuf(const VCBlock& vcBlock,
                       ZilliqaMessage::ProtoVCBlock& protoVCBlock);

bool ProtobufToVCBlock(const ZilliqaMessage::ProtoVCBlock& protoVCBlock,
                       VCBlock& vcBlock);

void MbInfoToProtobuf(const MicroBlockInfo& mbInfo,
                      ZilliqaMessage::ProtoMbInfo& ProtoMbInfo);

}  // namespace io

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_SERIALIZATION_H_
