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

#ifndef __DATACONVERSION_H__
#define __DATACONVERSION_H__

#include <array>
#include <boost/algorithm/hex.hpp>
#include <string>
#include <vector>

#include "common/Serializable.h"

/// Utility class for data conversion operations.
class DataConversion {
 public:
  /// Converts alphanumeric hex string to byte vector.
  static const bytes HexStrToUint8Vec(const std::string& hex_input);

  /// Converts alphanumeric hex string to 32-byte array.
  static const std::array<unsigned char, 32> HexStrToStdArray(
      const std::string& hex_input);

  /// Converts alphanumeric hex string to 64-byte array.
  static const std::array<unsigned char, 64> HexStrToStdArray64(
      const std::string& hex_input);

  /// Converts byte vector to alphanumeric hex string.
  static const std::string Uint8VecToHexStr(const bytes& hex_vec);

  /// Converts byte vector to alphanumeric hex string.
  static const std::string Uint8VecToHexStr(const bytes& hex_vec,
                                            unsigned int offset,
                                            unsigned int len);

  /// Converts fixed-sized byte array to alphanumeric hex string.
  template <size_t SIZE>
  static std::string charArrToHexStr(
      const std::array<unsigned char, SIZE>& hex_arr) {
    std::string str;
    boost::algorithm::hex(hex_arr.begin(), hex_arr.end(),
                          std::back_inserter(str));
    return str;
  }

  /// Converts a serializable object to alphanumeric hex string.
  static std::string SerializableToHexStr(const Serializable& input);

  static inline const std::string CharArrayToString(const bytes& v) {
    return std::string(v.begin(), v.end());
  }

  static inline const bytes StringToCharArray(const std::string& input) {
    return bytes(input.begin(), input.end());
  }

  static uint16_t charArrTo16Bits(const bytes& hex_arr);

  static uint32_t Pack(uint16_t a, uint16_t b) {
    return (int32_t)((((uint32_t)a) << 16) + (uint32_t)b);
  }

  static uint16_t UnpackA(uint32_t x) { return (uint16_t)(x >> 16); }

  static uint16_t UnpackB(uint32_t x) { return (uint16_t)(x & 0xffff); }
};

#endif  // __DATACONVERSION_H__
