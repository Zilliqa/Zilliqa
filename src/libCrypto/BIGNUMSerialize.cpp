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

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#include "Schnorr.h"
#include "libUtils/Logger.h"

using namespace std;

std::mutex BIGNUMSerialize::m_mutexBIGNUM;

shared_ptr<BIGNUM> BIGNUMSerialize::GetNumber(const bytes& src,
                                              unsigned int offset,
                                              unsigned int size) {
  // Check for offset overflow
  if ((offset + size) < size) {
    LOG_GENERAL(WARNING,
                "Overflow detected. offset = " << offset << " size = " << size);
    return nullptr;
  }

  if (offset + size > src.size()) {
    LOG_GENERAL(WARNING,
                "Can't get BIGNUM. offset = " << offset << " size = " << size
                                              << " src = " << src.size());
    return nullptr;
  }

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexBIGNUM);
  return shared_ptr<BIGNUM>(BN_bin2bn(src.data() + offset, size, NULL),
                            BN_clear_free);
}

void BIGNUMSerialize::SetNumber(bytes& dst, unsigned int offset,
                                unsigned int size, shared_ptr<BIGNUM> value) {
  // Check for offset overflow
  if ((offset + size) < size) {
    LOG_GENERAL(WARNING,
                "Overflow detected. offset = " << offset << " size = " << size);
    return;
  }

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexBIGNUM);

  const int actual_bn_size = BN_num_bytes(value.get());

  if (actual_bn_size <= static_cast<int>(size)) {
    if (offset + size > dst.size()) {
      dst.resize(offset + size);
    }

    // Pad with zeroes as needed
    const unsigned int size_diff =
        size - static_cast<unsigned int>(actual_bn_size);
    fill(dst.begin() + offset, dst.begin() + offset + size_diff, 0x00);

    if (BN_bn2bin(value.get(), dst.data() + offset + size_diff) !=
        actual_bn_size) {
      LOG_GENERAL(WARNING, "BN_bn2bin failed");
    }
  } else {
    LOG_GENERAL(WARNING, "BIGNUM size " << actual_bn_size << " > declared size "
                                        << size);
  }
}