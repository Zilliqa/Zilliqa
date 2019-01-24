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

// ============================================================================
// Construction
// ============================================================================

bool PubKey::constructPreChecks() {
  if (m_P == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    return false;
  }

  return true;
}

PubKey::PubKey()
    : m_P(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }
}

PubKey::PubKey(const PrivKey& privkey)
    : m_P(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }

  const Curve& curve = Schnorr::GetInstance().GetCurve();

  if (BN_is_zero(privkey.m_d.get()) ||
      (BN_cmp(privkey.m_d.get(), curve.m_order.get()) != -1)) {
    LOG_GENERAL(WARNING,
                "Input private key is invalid. Public key "
                "generation failed");
    return;
  }
  if (EC_POINT_mul(curve.m_group.get(), m_P.get(), privkey.m_d.get(), NULL,
                   NULL, NULL) == 0) {
    LOG_GENERAL(WARNING, "Public key generation failed");
    return;
  }
}

PubKey::PubKey(const bytes& src, unsigned int offset)
    : m_P(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }

  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init PubKey from stream");
  }
}

PubKey::PubKey(const PubKey& src)
    : m_P(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }

  if (!EC_POINT_copy(m_P.get(), src.m_P.get())) {
    LOG_GENERAL(WARNING, "PubKey copy failed");
    return;
  }
}

PubKey::~PubKey() {}

// ============================================================================
// Serialization
// ============================================================================

unsigned int PubKey::Serialize(bytes& dst, unsigned int offset) const {
  ECPOINTSerialize::SetNumber(dst, offset, PUB_KEY_SIZE, m_P);
  return PUB_KEY_SIZE;
}

PubKey PubKey::GetPubKeyFromString(const string& key) {
  if (key.size() != 66) {
    throw std::invalid_argument(
        "Error: public key - invalid number of input characters for key");
  }

  bytes key_v;

  if (!DataConversion::HexStrToUint8Vec(key, key_v)) {
    throw std::invalid_argument(
        "Error: public key - invalid format of input characters for key - "
        "required hexadecimal characters");
  }

  return PubKey(key_v, 0);
}

int PubKey::Deserialize(const bytes& src, unsigned int offset) {
  shared_ptr<EC_POINT> result =
      ECPOINTSerialize::GetNumber(src, offset, PUB_KEY_SIZE);

  if (result == nullptr) {
    LOG_GENERAL(WARNING, "ECPOINTSerialize::GetNumber failed");
    return -1;
  }

  if (!EC_POINT_copy(m_P.get(), result.get())) {
    LOG_GENERAL(WARNING, "PubKey copy failed");
    return -1;
  }

  return 0;
}

// ============================================================================
// Assignment and Comparison
// ============================================================================

PubKey& PubKey::operator=(const PubKey& src) {
  if (!EC_POINT_copy(m_P.get(), src.m_P.get())) {
    LOG_GENERAL(WARNING, "PubKey copy failed");
  }
  return *this;
}

bool PubKey::comparePreChecks(const PubKey& r, shared_ptr<BIGNUM>& lhs_bnvalue,
                              shared_ptr<BIGNUM>& rhs_bnvalue) const {
  unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    LOG_GENERAL(FATAL, "Memory allocation failure");
    return false;
  }

  lhs_bnvalue.reset(
      EC_POINT_point2bn(Schnorr::GetInstance().GetCurve().m_group.get(),
                        m_P.get(), POINT_CONVERSION_COMPRESSED, NULL,
                        ctx.get()),
      BN_clear_free);
  rhs_bnvalue.reset(
      EC_POINT_point2bn(Schnorr::GetInstance().GetCurve().m_group.get(),
                        r.m_P.get(), POINT_CONVERSION_COMPRESSED, NULL,
                        ctx.get()),
      BN_clear_free);

  if ((lhs_bnvalue == nullptr) || (rhs_bnvalue == nullptr)) {
    LOG_GENERAL(FATAL, "Memory allocation failure");
    return false;
  }

  return true;
}

bool PubKey::operator<(const PubKey& r) const {
  shared_ptr<BIGNUM> lhs_bnvalue, rhs_bnvalue;
  return comparePreChecks(r, lhs_bnvalue, rhs_bnvalue) &&
         BN_cmp(lhs_bnvalue.get(), rhs_bnvalue.get()) == -1;
}

bool PubKey::operator>(const PubKey& r) const { return r < *this; }

bool PubKey::operator==(const PubKey& r) const {
  shared_ptr<BIGNUM> lhs_bnvalue, rhs_bnvalue;
  return comparePreChecks(r, lhs_bnvalue, rhs_bnvalue) &&
         BN_cmp(lhs_bnvalue.get(), rhs_bnvalue.get()) == 0;
}