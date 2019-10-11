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

#ifndef ZILLIQA_SRC_LIBUTILS_CRYPTOUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_CRYPTOUTILS_H_

#include <Schnorr.h>
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Address.h"

namespace CryptoUtils {
Address GetAddressFromPubKey(const PubKey& pubKey) {
  bytes addr_ser;
  pubKey.Serialize(addr_ser, 0);
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(addr_ser, 0, PUB_KEY_SIZE);
  const bytes& tmp = sha2.Finalize();
  Address ret;
  copy(tmp.end() - ACC_ADDR_SIZE, tmp.end(), ret.asArray().begin());
  return ret;
}
}  // namespace CryptoUtils

#endif  // ZILLIQA_SRC_LIBUTILS_CRYPTOUTILS_H_
