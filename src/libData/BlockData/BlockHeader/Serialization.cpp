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

bool CheckRequiredFieldsProtoBlockHeaderBase(
    const ZilliqaMessage::ProtoBlockHeaderBase& /*protoBlockHeaderBase*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockHeaderBase.has_version() &&
         protoBlockHeaderBase.has_committeehash() &&
         protoBlockHeaderBase.has_prevhash();
#endif
  return true;
}

}  // namespace

void BlockHeaderBaseToProtobuf(
    const BlockHeaderBase& base,
    ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase) {
  // version
  protoBlockHeaderBase.set_version(base.GetVersion());
  // committee hash
  protoBlockHeaderBase.set_committeehash(base.GetCommitteeHash().data(),
                                         base.GetCommitteeHash().size);
  protoBlockHeaderBase.set_prevhash(base.GetPrevHash().data(),
                                    base.GetPrevHash().size);
}

std::optional<std::tuple<uint32_t, CommitteeHash, BlockHash>>
ProtobufToBlockHeaderBase(
    const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase) {
  if (!CheckRequiredFieldsProtoBlockHeaderBase(protoBlockHeaderBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockHeaderBase failed");
    return std::nullopt;
  }

  // Deserialize the version
  uint32_t version = protoBlockHeaderBase.version();

  // Deserialize committee hash
  CommitteeHash committeeHash;
  if (!CopyWithSizeCheck(protoBlockHeaderBase.committeehash(),
                         committeeHash.asArray())) {
    return std::nullopt;
  }

  // Deserialize prev hash
  BlockHash prevHash;
  if (!CopyWithSizeCheck(protoBlockHeaderBase.prevhash(), prevHash.asArray())) {
    return std::nullopt;
  }

  return std::make_tuple(version, committeeHash, prevHash);
}

}  // namespace io
