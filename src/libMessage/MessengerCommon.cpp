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

#include "MessengerCommon.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     Serializable& serializable) {
  zbytes tmp(byteArray.data().size());
  copy(byteArray.data().begin(), byteArray.data().end(), tmp.begin());
  return serializable.Deserialize(tmp, 0) == 0;
}

bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     SerializableCrypto& serializable) {
  zbytes tmp(byteArray.data().size());
  copy(byteArray.data().begin(), byteArray.data().end(), tmp.begin());
  return serializable.Deserialize(tmp, 0);
}

// Temporary function for use by data blocks
void SerializableToProtobufByteArray(const SerializableDataBlock& serializable,
                                     ZilliqaMessage::ByteArray& byteArray) {
  zbytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

// Temporary function for use by data blocks
bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     SerializableDataBlock& serializable) {
  return serializable.Deserialize(byteArray.data(), 0);
}
