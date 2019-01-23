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

std::mutex ECPOINTSerialize::m_mutexECPOINT;

shared_ptr<EC_POINT> ECPOINTSerialize::GetNumber(const bytes& src,
                                                 unsigned int offset,
                                                 unsigned int size) {
  shared_ptr<BIGNUM> bnvalue = BIGNUMSerialize::GetNumber(src, offset, size);

  if (bnvalue == nullptr) {
    LOG_GENERAL(WARNING, "BIGNUMSerialize::GetNumber failed");
    return nullptr;
  }

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexECPOINT);

  unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    LOG_GENERAL(FATAL, "Memory allocation failure");
    return nullptr;
  }

  return shared_ptr<EC_POINT>(
      EC_POINT_bn2point(Schnorr::GetInstance().GetCurve().m_group.get(),
                        bnvalue.get(), NULL, ctx.get()),
      EC_POINT_clear_free);
}

void ECPOINTSerialize::SetNumber(bytes& dst, unsigned int offset,
                                 unsigned int size,
                                 shared_ptr<EC_POINT> value) {
  shared_ptr<BIGNUM> bnvalue;

  {
    // This mutex is to prevent multi-threaded issues with the use of openssl
    // functions
    std::lock_guard<mutex> g(m_mutexECPOINT);

    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
    if (ctx == nullptr) {
      LOG_GENERAL(FATAL, "Memory allocation failure");
      return;
    }

    bnvalue.reset(
        EC_POINT_point2bn(Schnorr::GetInstance().GetCurve().m_group.get(),
                          value.get(), POINT_CONVERSION_COMPRESSED, NULL,
                          ctx.get()),
        BN_clear_free);
  }

  if (bnvalue == nullptr) {
    LOG_GENERAL(WARNING, "EC_POINT_point2bn failed");
    return;
  }

  BIGNUMSerialize::SetNumber(dst, offset, size, bnvalue);
}