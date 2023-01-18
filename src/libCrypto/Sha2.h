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

#ifndef ZILLIQA_SRC_LIBCRYPTO_SHA2_H_
#define ZILLIQA_SRC_LIBCRYPTO_SHA2_H_

#include <openssl/sha.h>
#include <string>
#include <vector>
#include "common/BaseType.h"
#include "common/FatalAssert.h"

/// Implements SHA2 hash algorithm.
template <unsigned int SIZE>
class SHA2 {
  static const constexpr unsigned int HASH_OUTPUT_SIZE = SIZE / 8;
  SHA256_CTX m_context{};
  zbytes m_output;

 public:
  /// Constructor.
  SHA2() : m_output(HASH_OUTPUT_SIZE) {
    static_assert(SIZE == 256, "Only SHA256 is currently supported");
    Reset();
  }

  /// Hash update function.
  void Update(const zbytes& input) {
    if (!input.empty()) SHA256_Update(&m_context, input.data(), input.size());
  }

  /// Hash update function.
  void Update(const zbytes& input, unsigned int offset, unsigned int size) {
    ZIL_FATAL_ASSERT((offset + size) <= input.size());

    SHA256_Update(&m_context, input.data() + offset, size);
  }

  /// Hash update function.
  void Update(const std::string& input) {
    if (!input.empty()) Update(input, 0, input.size());
  }

  /// Hash update function.
  void Update(const std::string& input, unsigned int offset,
              unsigned int size) {
    ZIL_FATAL_ASSERT((offset + size) <= input.size());

    SHA256_Update(&m_context, input.data() + offset, size);
  }

  /// Hash update function.
  void Update(const uint8_t* input, unsigned int size) {
    SHA256_Update(&m_context, input, size);
  }

  /// Resets the algorithm.
  void Reset() { SHA256_Init(&m_context); }

  /// Hash finalize function.
  zbytes Finalize() {
    SHA256_Final(m_output.data(), &m_context);
    return m_output;
  }

  static zbytes FromBytes(const zbytes& vec) {
    SHA2<SIZE> sha2;

    sha2.Update(vec);
    return sha2.Finalize();
  }
};

using SHA256Calculator = SHA2<256>;

#endif  // ZILLIQA_SRC_LIBCRYPTO_SHA2_H_
