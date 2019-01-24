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

bool Signature::constructPreChecks() {
  if ((m_r == nullptr) || (m_s == nullptr)) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    return false;
  }

  return true;
}

Signature::Signature()
    : m_r(BN_new(), BN_clear_free), m_s(BN_new(), BN_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }
}

Signature::Signature(const bytes& src, unsigned int offset)
    : m_r(BN_new(), BN_clear_free), m_s(BN_new(), BN_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }

  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init Signature from stream");
  }
}

Signature::Signature(const Signature& src)
    : m_r(BN_new(), BN_clear_free), m_s(BN_new(), BN_clear_free) {
  if (!constructPreChecks()) {
    LOG_GENERAL(FATAL, "constructPreChecks failed");
    return;
  }

  if (BN_copy(m_r.get(), src.m_r.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature challenge copy failed");
    return;
  }

  if (BN_copy(m_s.get(), src.m_s.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature response copy failed");
  }
}

Signature::~Signature() {}

// ============================================================================
// Serialization
// ============================================================================

unsigned int Signature::Serialize(bytes& dst, unsigned int offset) const {
  BIGNUMSerialize::SetNumber(dst, offset, SIGNATURE_CHALLENGE_SIZE, m_r);
  BIGNUMSerialize::SetNumber(dst, offset + SIGNATURE_CHALLENGE_SIZE,
                             SIGNATURE_RESPONSE_SIZE, m_s);
  return SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;
}

int Signature::Deserialize(const bytes& src, unsigned int offset) {
  shared_ptr<BIGNUM> result_r =
      BIGNUMSerialize::GetNumber(src, offset, SIGNATURE_CHALLENGE_SIZE);
  shared_ptr<BIGNUM> result_s = BIGNUMSerialize::GetNumber(
      src, offset + SIGNATURE_CHALLENGE_SIZE, SIGNATURE_RESPONSE_SIZE);

  if ((result_r == nullptr) || (result_s == nullptr)) {
    LOG_GENERAL(WARNING, "BIGNUMSerialize::GetNumber failed");
    return -1;
  }

  if (BN_copy(m_r.get(), result_r.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature challenge copy failed");
    return -1;
  }

  if (BN_copy(m_s.get(), result_s.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature response copy failed");
    return -1;
  }

  return 0;
}

// ============================================================================
// Assignment and Comparison
// ============================================================================

Signature& Signature::operator=(const Signature& src) {
  if (BN_copy(m_r.get(), src.m_r.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature challenge copy failed");
  }

  if (BN_copy(m_s.get(), src.m_s.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature response copy failed");
  }

  return *this;
}

bool Signature::operator==(const Signature& r) const {
  return (BN_cmp(m_r.get(), r.m_r.get()) == 0) &&
         (BN_cmp(m_s.get(), r.m_s.get()) == 0);
}