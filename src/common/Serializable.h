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

#ifndef __SERIALIZABLE_H__
#define __SERIALIZABLE_H__

#include "BaseType.h"

/// Specifies the interface required for classes that are byte serializable.
class Serializable {
 public:
  /// Serializes internal state to destination byte stream.
  virtual unsigned int Serialize(bytes& dst, unsigned int offset) const = 0;

  /// Deserializes source byte stream into internal state.
  virtual int Deserialize(const bytes& src, unsigned int offset) = 0;

  /// Virtual destructor.
  virtual ~Serializable() {}

  /// Template function for extracting a number from the source byte stream at
  /// the specified offset. Returns 0 if there are not enough bytes to read from
  /// the stream.
  template <class numerictype>
  static numerictype GetNumber(const bytes& src, unsigned int offset,
                               unsigned int numerictype_len) {
    numerictype result = 0;

    if (offset + numerictype_len <= src.size()) {
      unsigned int left_shift = (numerictype_len - 1) * 8;
      for (unsigned int i = 0; i < numerictype_len; i++) {
        numerictype tmp = src.at(offset + i);
        result += (tmp << left_shift);
        left_shift -= 8;
      }
    }

    return result;
  }

  /// Template function for placing a number into the destination byte stream at
  /// the specified offset. Destination is resized if necessary.
  template <class numerictype>
  static void SetNumber(bytes& dst, unsigned int offset, numerictype value,
                        unsigned int numerictype_len) {
    const unsigned int length_available = dst.size() - offset;

    if (length_available < numerictype_len) {
      dst.resize(dst.size() + numerictype_len - length_available);
    }

    unsigned int right_shift = (numerictype_len - 1) * 8;
    for (unsigned int i = 0; i < numerictype_len; i++) {
      dst.at(offset + i) = static_cast<uint8_t>((value >> right_shift) & 0xFF);
      right_shift -= 8;
    }
  }
};

// This is a temporary class for use with data blocks
class SerializableDataBlock {
 public:
  /// Serializes internal state to destination byte stream.
  virtual bool Serialize(bytes& dst, unsigned int offset) const = 0;

  /// Deserializes source byte stream into internal state.
  virtual bool Deserialize(const bytes& src, unsigned int offset) = 0;

  /// Virtual destructor.
  virtual ~SerializableDataBlock() {}

  /// Template function for extracting a number from the source byte stream at
  /// the specified offset. Returns 0 if there are not enough bytes to read from
  /// the stream.
  template <class numerictype>
  static numerictype GetNumber(const bytes& src, unsigned int offset,
                               unsigned int numerictype_len) {
    numerictype result = 0;

    if (offset + numerictype_len <= src.size()) {
      unsigned int left_shift = (numerictype_len - 1) * 8;
      for (unsigned int i = 0; i < numerictype_len; i++) {
        numerictype tmp = src.at(offset + i);
        result += (tmp << left_shift);
        left_shift -= 8;
      }
    }

    return result;
  }

  /// Template function for placing a number into the destination byte stream at
  /// the specified offset. Destination is resized if necessary.
  template <class numerictype>
  static void SetNumber(bytes& dst, unsigned int offset, numerictype value,
                        unsigned int numerictype_len) {
    const unsigned int length_available = dst.size() - offset;

    if (length_available < numerictype_len) {
      dst.resize(dst.size() + numerictype_len - length_available);
    }

    unsigned int right_shift = (numerictype_len - 1) * 8;
    for (unsigned int i = 0; i < numerictype_len; i++) {
      dst.at(offset + i) = static_cast<uint8_t>((value >> right_shift) & 0xFF);
      right_shift -= 8;
    }
  }
};

#endif  // __SERIALIZABLE_H__
