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

namespace io {

namespace {

inline bool CheckRequiredFieldsProtoBlockBase(
    const ZilliqaMessage::ProtoBlockBase& /*protoBlockBase*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockBase.has_blockhash() && protoBlockBase.has_cosigs() &&
         protoBlockBase.has_timestamp() &&
         CheckRequiredFieldsProtoBlockBaseCoSignatures(protoBlockBase.cosigs());
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

}  // namespace io
