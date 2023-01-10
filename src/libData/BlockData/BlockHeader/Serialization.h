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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_SERIALIZATION_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_SERIALIZATION_H_

#include "BlockHeaderBase.h"
#include "libMessage/MessengerCommon.h"

class DSBlockHeader;
class MicroBlockHeader;
class TxBlockHeader;
class VCBlockHeader;

namespace io {

void BlockHeaderBaseToProtobuf(
    const BlockHeaderBase& base,
    ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase);

std::optional<std::tuple<uint32_t, CommitteeHash, BlockHash>>
ProtobufToBlockHeaderBase(
    const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase);

void DSBlockHeaderToProtobuf(
    const DSBlockHeader& dsBlockHeader,
    ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
    bool concreteVarsOnly = false);

bool ProtobufToDSBlockHeader(
    const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
    DSBlockHeader& dsBlockHeader);

void MicroBlockHeaderToProtobuf(
    const MicroBlockHeader& microBlockHeader,
    ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader& protoMicroBlockHeader);

bool ProtobufToMicroBlockHeader(
    const ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader&
        protoMicroBlockHeader,
    MicroBlockHeader& microBlockHeader);

void TxBlockHeaderToProtobuf(
    const TxBlockHeader& txBlockHeader,
    ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoTxBlockHeader);

bool ProtobufToTxBlockHeader(
    const ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoTxBlockHeader,
    TxBlockHeader& txBlockHeader);

void VCBlockHeaderToProtobuf(
    const VCBlockHeader& vcBlockHeader,
    ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoVCBlockHeader);

bool ProtobufToVCBlockHeader(
    const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoVCBlockHeader,
    VCBlockHeader& vcBlockHeader);

}  // namespace io

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_SERIALIZATION_H_
