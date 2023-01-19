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
#include "libUtils/Logger.h"

#include <algorithm>
#include <ranges>

#define PROTOBUFBYTEARRAYTOSERIALIZABLE(ba, s)                       \
  if (!ProtobufByteArrayToSerializable(ba, s)) {                     \
    LOG_GENERAL(WARNING, "ProtobufByteArrayToSerializable failed."); \
    return false;                                                    \
  }

#define PROTOBUFBYTEARRAYTOSERIALIZABLEOPT(ba, s)                    \
  if (!ProtobufByteArrayToSerializable(ba, s)) {                     \
    LOG_GENERAL(WARNING, "ProtobufByteArrayToSerializable failed."); \
    return std::nullopt;                                             \
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

template <class T>
void SerializableToProtobufByteArray(const T& serializable,
                                     ZilliqaMessage::ByteArray& byteArray) {
  zbytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

template <class T, size_t S>
void ProtobufByteArrayToNumber(const ZilliqaMessage::ByteArray& byteArray,
                               T& number) {
  zbytes tmp(byteArray.data().size());
  copy(byteArray.data().begin(), byteArray.data().end(), tmp.begin());
  number = Serializable::GetNumber<T>(tmp, 0, S);
}

template <class T, size_t S>
void NumberToProtobufByteArray(const T& number,
                               ZilliqaMessage::ByteArray& byteArray) {
  zbytes tmp;
  Serializable::SetNumber<T>(tmp, 0, number, S);
  byteArray.set_data(tmp.data(), tmp.size());
}

template <class T>
bool SerializeToArray(const T& protoMessage, zbytes& dst,
                      const unsigned int offset) {
  if ((offset + protoMessage.ByteSizeLong()) > dst.size()) {
    dst.resize(offset + protoMessage.ByteSizeLong());
  }

  return protoMessage.SerializeToArray(dst.data() + offset,
                                       protoMessage.ByteSizeLong());
}

#if defined(__APPLE__) || __GNUC__ < 11
template <typename InputRangeT,
          typename OuputRangeT>
#else
template <std::ranges::input_range InputRangeT,
          std::ranges::contiguous_range OuputRangeT>
#endif
bool CopyWithSizeCheck(const InputRangeT& src, OuputRangeT& result) {
  // Fixed length copying.
  if (std::size(result) != std::size(src)) {
    LOG_GENERAL(WARNING, "Size check while copying failed. Size expected = "
                             << std::size(result)
                             << ", actual = " << std::size(src));
    return false;
  }

  std::copy(std::begin(src), std::end(src), std::begin(result));
  return true;
}

#endif  // ZILLIQA_SRC_LIBMESSAGE_MESSENGERCOMMON_H_
