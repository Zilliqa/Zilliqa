/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
