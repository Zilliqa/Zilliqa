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

#ifndef __SHA2_H__
#define __SHA2_H__

#include <openssl/sha.h>
#include <vector>
#include "libUtils/Logger.h"

/// List of supported hash variants.
class HASH_TYPE {
 public:
  static const unsigned int HASH_VARIANT_256 = 256;
  static const unsigned int HASH_VARIANT_512 = 512;
};

/// Implements SHA2 hash algorithm.
template <unsigned int SIZE>
class SHA2 {
  static const unsigned int HASH_OUTPUT_SIZE = SIZE / 8;
  SHA256_CTX m_context;
  bytes output;

 public:
  /// Constructor.
  SHA2() : output(HASH_OUTPUT_SIZE) {
    if (SIZE != HASH_TYPE::HASH_VARIANT_256) {
      LOG_GENERAL(FATAL, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
    }

    Reset();
  }

  /// Destructor.
  ~SHA2() {}

  /// Hash update function.
  void Update(const bytes& input) {
    if (input.size() == 0) {
      LOG_GENERAL(WARNING, "Nothing to update");
      return;
    }

    SHA256_Update(&m_context, input.data(), input.size());
  }

  /// Hash update function.
  void Update(const bytes& input, unsigned int offset, unsigned int size) {
    if ((offset + size) > input.size()) {
      LOG_GENERAL(FATAL, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
    }

    SHA256_Update(&m_context, input.data() + offset, size);
  }

  /// Resets the algorithm.
  void Reset() { SHA256_Init(&m_context); }

  /// Hash finalize function.
  bytes Finalize() {
    switch (SIZE) {
      case 256:
        SHA256_Final(output.data(), &m_context);
        break;
      default:
        break;
    }
    return output;
  }
};

#endif  // __SHA2_H__
