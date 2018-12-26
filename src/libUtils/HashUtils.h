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
