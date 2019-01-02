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

#ifndef __HASH_UTILS__
#define __HASH_UTILS__

#include <string>
#include <vector>
#include "common/Serializable.h"
#include "libCrypto/Sha2.h"

class HashUtils {
 public:
  static const bytes SerializableToHash(const Serializable& sz) {
    bytes vec;
    sz.Serialize(vec, 0);
    return BytesToHash(vec);
  }
  // Temporary function for use by data blocks
  static const bytes SerializableToHash(const SerializableDataBlock& sz) {
    bytes vec;
    sz.Serialize(vec, 0);
    return BytesToHash(vec);
  }
  static const bytes BytesToHash(const bytes& vec) {
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

    sha2.Update(vec);
    const bytes& resVec = sha2.Finalize();

    return resVec;
  }
  static uint16_t SerializableToHash16Bits(const Serializable& sz) {
    const bytes& vec = SerializableToHash(sz);

    if (vec.size() == 0) {
      return 0;
    }

    uint32_t lsb = vec.size() - 1;

    return (vec.at(lsb - 1) << 8) | vec.at(lsb);
  }
  // Temporary function for use by data blocks
  static uint16_t SerializableToHash16Bits(const SerializableDataBlock& sz) {
    const bytes& vec = SerializableToHash(sz);

    if (vec.size() == 0) {
      return 0;
    }

    uint32_t lsb = vec.size() - 1;

    return (vec.at(lsb - 1) << 8) | vec.at(lsb);
  }
};

#endif  //__HASH_UTILS__
