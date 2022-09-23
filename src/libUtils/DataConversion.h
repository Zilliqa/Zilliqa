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

#ifndef ZILLIQA_SRC_LIBUTILS_DATACONVERSION_H_
#define ZILLIQA_SRC_LIBUTILS_DATACONVERSION_H_

#include <array>
#include <boost/algorithm/hex.hpp>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include <Schnorr.h>

#include "common/Serializable.h"
#include "libUtils/Logger.h"

/// Utility class for data conversion operations.
class DataConversion {
 public:
  /// Converts alphanumeric hex string to Uint64.
  static bool HexStringToUint64(const std::string& s, uint64_t* res) {
    try {
      *res = std::stoull(s, nullptr, 16);
    } catch (const std::invalid_argument& e) {
      LOG_GENERAL(WARNING, "Convert failed, invalid input: " << s);
      return false;
    } catch (const std::out_of_range& e) {
      LOG_GENERAL(WARNING, "Convert failed, out of range: " << s);
      return false;
    }
    return true;
  }

  static uint64_t HexStringToUint64Ret(const std::string& s) {
    uint64_t ret = 0;

    if (s.size() > 2 && s[1] == 'x') {
      HexStringToUint64(std::string(s.c_str() + 2), &ret);
    } else {
      HexStringToUint64(s, &ret);
    }

    return ret;
  }

  /// Converts alphanumeric hex string to byte vector.
  static bool HexStrToUint8Vec(const std::string& hex_input, bytes& out);

  static bytes HexStrToUint8VecRet(const std::string& hex_input);

  /// Converts alphanumeric hex string to 32-byte array.
  static bool HexStrToStdArray(const std::string& hex_input,
                               std::array<uint8_t, 32>& d);

  /// Converts alphanumeric hex string to 64-byte array.
  static bool HexStrToStdArray64(const std::string& hex_input,
                                 std::array<uint8_t, 64>& d);

  /// Converts string to alphanumeric hex string.
  static bool StringToHexStr(const std::string& hex_str, std::string& str);

  /// Converts byte vector to alphanumeric hex string.
  static bool Uint8VecToHexStr(const bytes& hex_vec, std::string& str);

  static std::string Uint8VecToHexStrRet(const bytes& hex_vec);

  /// Converts byte vector to alphanumeric hex string.
  static bool Uint8VecToHexStr(const bytes& hex_vec, unsigned int offset,
                               unsigned int len, std::string& str);

  /// Converts fixed-sized byte array to alphanumeric hex string.
  template <size_t SIZE>
  static bool charArrToHexStr(const std::array<uint8_t, SIZE>& hex_arr,
                              std::string& str) {
    try {
      str = "";
      boost::algorithm::hex(hex_arr.begin(), hex_arr.end(),
                            std::back_inserter(str));
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING, "Failed charArrToHexStr conversion");
      return false;
    }
    return true;
  }

  /// Converts a serializable object to alphanumeric hex string.
  static bool SerializableToHexStr(const Serializable& input, std::string& str);

  /// Converts a serializable object in libSchnorr to alphanumeric hex string.
  static bool SerializableToHexStr(const SerializableCrypto& input,
                                   std::string& str);

  static inline const std::string CharArrayToString(const bytes& v) {
    return std::string(v.begin(), v.end());
  }

  static inline const std::vector<uint8_t> StringToCharArray(
      const std::string& input) {
    return bytes(input.begin(), input.end());
  }

  static uint16_t charArrTo16Bits(const bytes& hex_arr);

  static uint32_t Pack(uint16_t a, uint16_t b) {
    return (int32_t)((((uint32_t)a) << 16) + (uint32_t)b);
  }

  static uint16_t UnpackA(uint32_t x) { return (uint16_t)(x >> 16); }

  static uint16_t UnpackB(uint32_t x) { return (uint16_t)(x & 0xffff); }

  template <typename T, size_t SIZE>
  static std::string IntegerToHexString(T value) {
    std::stringstream ss;
    if (SIZE > sizeof(uint8_t)) {
      ss << std::hex << value;
    } else {
      ss << std::hex << (uint32_t)value;
    }
    std::string strResult = ss.str();
    auto resultSize = SIZE * 2;
    if (strResult.length() < resultSize) {
      strResult = std::string(resultSize - strResult.length(), '0') + strResult;
    }
    return strResult;
  }

  template <typename T, size_t SIZE>
  static bytes IntegerToBytes(T value) {
    bytes result(SIZE);
    for (size_t i = 0; i < SIZE; i++) {
      result[SIZE - i - 1] = (value >> (i * 8));
    }
    return result;
  }

  /// Normalize alphanumeric hex string to lower and remove prefix "0x"
  static bool NormalizeHexString(std::string& s);

  /// Add '0x' to string if it does not exist already
  static std::string AddOXPrefix(std::string&& s);

  static size_t clz(uint8_t x) {
    static constexpr std::uint8_t clz_lookup[16] = {4, 3, 2, 2, 1, 1, 1, 1,
                                                    0, 0, 0, 0, 0, 0, 0, 0};
    auto upper = (x >> 4) & 0x0F;
    auto lower = x & 0x0F;
    return upper ? clz_lookup[upper] : 4 + clz_lookup[lower];
  }

  template <typename T>
  static std::string IntToHexString(T number, bool withX = true) {
    std::stringstream stream;

    if (withX) {
      stream << "0x" << std::hex << (int)number;
    } else {
      stream << std::hex << (int)number;
    }

    return stream.str();
  }
};

#endif  // ZILLIQA_SRC_LIBUTILS_DATACONVERSION_H_
