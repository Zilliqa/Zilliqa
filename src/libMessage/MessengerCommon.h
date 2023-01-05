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
#ifndef ZILLIQA_SRC_LIBMESSAGE_MESSENGERCOMMON_H_
#define ZILLIQA_SRC_LIBMESSAGE_MESSENGERCOMMON_H_

#include <Schnorr.h>
#include "common/Serializable.h"
#include "libMessage/ZilliqaMessage.pb.h"

#define PROTOBUFBYTEARRAYTOSERIALIZABLE(ba, s)                       \
  if (!ProtobufByteArrayToSerializable(ba, s)) {                     \
    LOG_GENERAL(WARNING, "ProtobufByteArrayToSerializable failed."); \
    return false;                                                    \
  }

namespace ZilliqaMessage {
class ByteArray;
}

bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     Serializable& serializable);

bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     SerializableCrypto& serializable);

void SerializableToProtobufByteArray(const SerializableDataBlock& serializable,
                                     ZilliqaMessage::ByteArray& byteArray);

bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     SerializableDataBlock& serializable);

template <class T, size_t S>
void ProtobufByteArrayToNumber(const ZilliqaMessage::ByteArray& byteArray, T& number) {
  zbytes tmp(byteArray.data().size());
  copy(byteArray.data().begin(), byteArray.data().end(), tmp.begin());
  number = Serializable::GetNumber<T>(tmp, 0, S);
}

#endif  // ZILLIQA_SRC_LIBMESSAGE_MESSENGERCOMMON_H_
