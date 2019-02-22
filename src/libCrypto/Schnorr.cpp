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

/*
 * This file implements the Schnorr signature standard from
 * https://www.bsi.bund.de/SharedDocs/Downloads/EN/BSI/Publications/TechGuidelines/TR03111/BSI-TR-03111_pdf.pdf?__blob=publicationFile&v=1
 * Refer to Section 4.2.3, page 24.
 **/

#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/opensslv.h>
#include "Sha2.h"

#if OPENSSL_VERSION_NUMBER < 0x1010007fL  // only needed before OpenSSL 1.1.0g
#include "generate_dsa_nonce.h"
#endif

#include <array>

#include "Schnorr.h"
#include "libUtils/Logger.h"

using namespace std;

Curve::Curve()
    : m_group(EC_GROUP_new_by_curve_name(NID_secp256k1), EC_GROUP_clear_free),
      m_order(BN_new(), BN_clear_free) {
  if (m_group == nullptr) {
    LOG_GENERAL(FATAL, "Curve group setup failed");
    return;
  }
  if (m_order == nullptr) {
    LOG_GENERAL(FATAL, "Curve order setup failed");
    return;
  }
  // Get group order
  if (!EC_GROUP_get_order(m_group.get(), m_order.get(), NULL)) {
    LOG_GENERAL(FATAL, "Recover curve order failed");
  }
}

Curve::~Curve() {}

Schnorr::Schnorr() {}

Schnorr::~Schnorr() {}

Schnorr& Schnorr::GetInstance() {
  static Schnorr schnorr;
  return schnorr;
}

const Curve& Schnorr::GetCurve() const { return m_curve; }

PairOfKey Schnorr::GenKeyPair() {
  // LOG_MARKER();

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexSchnorr);

  PrivKey privkey;
  PubKey pubkey(privkey);

  return make_pair(PrivKey(privkey), PubKey(pubkey));
}

bool Schnorr::Sign(const bytes& message, const PrivKey& privkey,
                   const PubKey& pubkey, Signature& result) {
  return Sign(message, 0, message.size(), privkey, pubkey, result);
}

bool Schnorr::Sign(const bytes& message, unsigned int offset, unsigned int size,
                   const PrivKey& privkey, const PubKey& pubkey,
                   Signature& result) {
  // LOG_MARKER();

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexSchnorr);

  // Initial checks

  if (message.size() == 0) {
    LOG_GENERAL(WARNING, "Empty message");
    return false;
  }

  if (message.size() < (offset + size)) {
    LOG_GENERAL(WARNING, "Offset and size beyond message size");
    return false;
  }

  // Main signing procedure

  // The algorithm takes the following steps:
  // 1. Generate a random k from [1, ..., order-1]
  // 2. Compute the commitment Q = kG, where  G is the base point
  // 3. Compute the challenge r = H(Q, kpub, m)
  // 4. If r = 0 mod(order), goto 1
  // 5. Compute s = k - r*kpriv mod(order)
  // 6. If s = 0 goto 1.
  // 7  Signature on m is (r, s)

  bytes buf(PUBKEY_COMPRESSED_SIZE_BYTES);
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  bool err = false;  // detect error
  int res = 1;       // result to return

  unique_ptr<BIGNUM, void (*)(BIGNUM*)> k(BN_new(), BN_clear_free);
  unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
      EC_POINT_new(m_curve.m_group.get()), EC_POINT_clear_free);
  unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

  if ((k != nullptr) && (ctx != nullptr) && (Q != nullptr)) {
    do {
      err = false;

      // 1. Generate a random k from [1,..., order-1]
      do {
        err = (BN_generate_dsa_nonce(
                   k.get(), m_curve.m_order.get(), privkey.m_d.get(),
                   static_cast<const unsigned char*>(message.data()),
                   message.size(), ctx.get()) == 0);

        // err =
        // (BN_rand(k.get(), BN_num_bits(m_curve.m_order.get()), -1, 0) == 0);
        if (err) {
          LOG_GENERAL(WARNING, "Random generation failed");
          return false;
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses-equality"
      } while (BN_is_zero(k.get()));
#pragma clang diagnostic pop

      // 2. Compute the commitment Q = kG, where G is the base point
      err = (EC_POINT_mul(m_curve.m_group.get(), Q.get(), k.get(), NULL, NULL,
                          NULL) == 0);
      if (err) {
        LOG_GENERAL(WARNING, "Commit generation failed");
        return false;
      }

      // 3. Compute the challenge r = H(Q, kpub, m)

      // Convert the committment to octets first
      err = (EC_POINT_point2oct(m_curve.m_group.get(), Q.get(),
                                POINT_CONVERSION_COMPRESSED, buf.data(),
                                PUBKEY_COMPRESSED_SIZE_BYTES,
                                NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
      if (err) {
        LOG_GENERAL(WARNING, "Commit octet conversion failed");
        return false;
      }

      // Hash commitment
      sha2.Update(buf);

      // Clear buffer
      fill(buf.begin(), buf.end(), 0x00);

      // Convert the public key to octets
      err = (EC_POINT_point2oct(m_curve.m_group.get(), pubkey.m_P.get(),
                                POINT_CONVERSION_COMPRESSED, buf.data(),
                                PUBKEY_COMPRESSED_SIZE_BYTES,
                                NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
      if (err) {
        LOG_GENERAL(WARNING, "Pubkey octet conversion failed");
        return false;
      }

      // Hash public key
      sha2.Update(buf);

      // Hash message
      sha2.Update(message, offset, size);
      bytes digest = sha2.Finalize();

      // Build the challenge
      err =
          ((BN_bin2bn(digest.data(), digest.size(), result.m_r.get())) == NULL);
      if (err) {
        LOG_GENERAL(WARNING, "Digest to challenge failed");
        return false;
      }

      err = (BN_nnmod(result.m_r.get(), result.m_r.get(), m_curve.m_order.get(),
                      NULL) == 0);
      if (err) {
        LOG_GENERAL(WARNING, "BIGNUM NNmod failed");
        return false;
      }

      // 4. Compute s = k - r*krpiv
      // 4.1 r*kpriv
      err = (BN_mod_mul(result.m_s.get(), result.m_r.get(), privkey.m_d.get(),
                        m_curve.m_order.get(), ctx.get()) == 0);
      if (err) {
        LOG_GENERAL(WARNING, "Response mod mul failed");
        return false;
      }

      // 4.2 k-r*kpriv
      err = (BN_mod_sub(result.m_s.get(), k.get(), result.m_s.get(),
                        m_curve.m_order.get(), ctx.get()) == 0);
      if (err) {
        LOG_GENERAL(WARNING, "BIGNUM mod sub failed");
        return false;
      }

      // Clear buffer
      fill(buf.begin(), buf.end(), 0x00);

      if (!err) {
        res = (BN_is_zero(result.m_r.get())) || (BN_is_zero(result.m_s.get()));
      }

      sha2.Reset();
    } while (res);
  } else {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    return false;
    // throw exception();
  }

  return (res == 0);
}

bool Schnorr::Verify(const bytes& message, const Signature& toverify,
                     const PubKey& pubkey) {
  return Verify(message, 0, message.size(), toverify, pubkey);
}

bool Schnorr::Verify(const bytes& message, unsigned int offset,
                     unsigned int size, const Signature& toverify,
                     const PubKey& pubkey) {
  // LOG_MARKER();

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexSchnorr);

  // Initial checks

  if (message.size() == 0) {
    LOG_GENERAL(WARNING, "Empty message");
    return false;
  }

  if (message.size() < (offset + size)) {
    LOG_GENERAL(WARNING, "Offset and size beyond message size");
    return false;
  }

  try {
    // Main verification procedure

    // The algorithm to check the signature (r, s) on a message m using a public
    // key kpub is as follows
    // 1. Check if r,s is in [1, ..., order-1]
    // 2. Compute Q = sG + r*kpub
    // 3. If Q = O (the neutral point), return 0;
    // 4. r' = H(Q, kpub, m)
    // 5. return r' == r

    bytes buf(PUBKEY_COMPRESSED_SIZE_BYTES);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

    bool err = false;
    bool err2 = false;

    // Regenerate the commitmment part of the signature
    unique_ptr<BIGNUM, void (*)(BIGNUM*)> challenge_built(BN_new(),
                                                          BN_clear_free);
    unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
        EC_POINT_new(m_curve.m_group.get()), EC_POINT_clear_free);
    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

    if ((challenge_built != nullptr) && (ctx != nullptr) && (Q != nullptr)) {
      // 1. Check if r,s is in [1, ..., order-1]
      err2 = (BN_is_zero(toverify.m_r.get()) ||
              BN_is_negative(toverify.m_r.get()) ||
              (BN_cmp(toverify.m_r.get(), m_curve.m_order.get()) != -1));
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Challenge not in range");
        return false;
      }

      err2 = (BN_is_zero(toverify.m_s.get()) ||
              BN_is_negative(toverify.m_s.get()) ||
              (BN_cmp(toverify.m_s.get(), m_curve.m_order.get()) != -1));
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Response not in range");
        return false;
      }

      // 2. Compute Q = sG + r*kpub
      err2 =
          (EC_POINT_mul(m_curve.m_group.get(), Q.get(), toverify.m_s.get(),
                        pubkey.m_P.get(), toverify.m_r.get(), ctx.get()) == 0);
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Commit regenerate failed");
        return false;
      }

      // 3. If Q = O (the neutral point), return 0;
      err2 = (EC_POINT_is_at_infinity(m_curve.m_group.get(), Q.get()));
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Commit at infinity");
        return false;
      }

      // 4. r' = H(Q, kpub, m)
      // 4.1 Convert the committment to octets first
      err2 = (EC_POINT_point2oct(m_curve.m_group.get(), Q.get(),
                                 POINT_CONVERSION_COMPRESSED, buf.data(),
                                 PUBKEY_COMPRESSED_SIZE_BYTES,
                                 NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Commit octet conversion failed");
        return false;
      }

      // Hash commitment
      sha2.Update(buf);

      // Reset buf
      fill(buf.begin(), buf.end(), 0x00);

      // 4.2 Convert the public key to octets
      err2 = (EC_POINT_point2oct(m_curve.m_group.get(), pubkey.m_P.get(),
                                 POINT_CONVERSION_COMPRESSED, buf.data(),
                                 PUBKEY_COMPRESSED_SIZE_BYTES,
                                 NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Pubkey octet conversion failed");
        return false;
      }

      // Hash public key
      sha2.Update(buf);

      // 4.3 Hash message
      sha2.Update(message, offset, size);
      bytes digest = sha2.Finalize();

      // 5. return r' == r
      err2 = (BN_bin2bn(digest.data(), digest.size(), challenge_built.get()) ==
              NULL);
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Challenge bin2bn conversion failed");
        return false;
      }

      err2 = (BN_nnmod(challenge_built.get(), challenge_built.get(),
                       m_curve.m_order.get(), NULL) == 0);
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Challenge rebuild mod failed");
        return false;
      }

      sha2.Reset();
    } else {
      LOG_GENERAL(WARNING, "Memory allocation failure");
      // throw exception();
      return false;
    }
    return (!err) && (BN_cmp(challenge_built.get(), toverify.m_r.get()) == 0);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with Schnorr::Verify." << ' ' << e.what());
    return false;
  }
}

void Schnorr::PrintPoint(const EC_POINT* point) {
  LOG_MARKER();

  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexSchnorr);

  unique_ptr<BIGNUM, void (*)(BIGNUM*)> x(BN_new(), BN_clear_free);
  unique_ptr<BIGNUM, void (*)(BIGNUM*)> y(BN_new(), BN_clear_free);

  if ((x != nullptr) && (y != nullptr)) {
    // Get affine coordinates for the point
    if (EC_POINT_get_affine_coordinates_GFp(m_curve.m_group.get(), point,
                                            x.get(), y.get(), NULL)) {
      unique_ptr<char, void (*)(void*)> x_str(BN_bn2hex(x.get()), free);
      unique_ptr<char, void (*)(void*)> y_str(BN_bn2hex(y.get()), free);

      if ((x_str != nullptr) && (y_str != nullptr)) {
        LOG_GENERAL(INFO, "x: " << x_str.get());
        LOG_GENERAL(INFO, "y: " << y_str.get());
      }
    }
  }
}
