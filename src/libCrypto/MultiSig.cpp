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

#include "MultiSig.h"
#include "Sha2.h"
#include "libUtils/Logger.h"

using namespace std;

static const uint8_t SECOND_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE = 0x01;
static const uint8_t THIRD_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE = 0x11;

CommitSecret::CommitSecret()
    : m_s(BN_new(), BN_clear_free), m_initialized(false) {
  // commit->secret should be in [1,...,order-1]

  if (m_s == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }

  bool err = false;

  do {
    const Curve& curve = Schnorr::GetInstance().GetCurve();
    err = (BN_rand_range(m_s.get(), curve.m_order.get()) == 0);
    if (err) {
      LOG_GENERAL(WARNING, "Value to commit rand failed");
      break;
    }
  } while (BN_is_zero(m_s.get()));

  m_initialized = (!err);
}

CommitSecret::CommitSecret(const bytes& src, unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init CommitSecret.");
  }
}

CommitSecret::CommitSecret(const CommitSecret& src)
    : m_s(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_s != nullptr) {
    if (BN_copy(m_s.get(), src.m_s.get()) == NULL) {
      LOG_GENERAL(WARNING, "CommitSecret copy failed");
    } else {
      m_initialized = true;
    }
  } else {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

CommitSecret::~CommitSecret() {}

bool CommitSecret::Initialized() const { return m_initialized; }

unsigned int CommitSecret::Serialize(bytes& dst, unsigned int offset) const {
  // LOG_MARKER();

  if (m_initialized) {
    BIGNUMSerialize::SetNumber(dst, offset, COMMIT_SECRET_SIZE, m_s);
  }

  return COMMIT_SECRET_SIZE;
}

int CommitSecret::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  try {
    m_s = BIGNUMSerialize::GetNumber(src, offset, COMMIT_SECRET_SIZE);
    if (m_s == nullptr) {
      LOG_GENERAL(WARNING, "Deserialization failure");
      m_initialized = false;
    } else {
      m_initialized = true;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with CommitSecret::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

CommitSecret& CommitSecret::operator=(const CommitSecret& src) {
  m_initialized = (BN_copy(m_s.get(), src.m_s.get()) == m_s.get());
  return *this;
}

bool CommitSecret::operator==(const CommitSecret& r) const {
  return (m_initialized && r.m_initialized &&
          (BN_cmp(m_s.get(), r.m_s.get()) == 0));
}

CommitPoint::CommitPoint()
    : m_p(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free),
      m_initialized(false) {
  if (m_p == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

CommitPoint::CommitPoint(const CommitSecret& secret)
    : m_p(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free),
      m_initialized(false) {
  if (m_p == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  } else {
    Set(secret);
  }
}

CommitPoint::CommitPoint(const bytes& src, unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init CommitPoint.");
  }
}

CommitPoint::CommitPoint(const CommitPoint& src)
    : m_p(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free),
      m_initialized(false) {
  if (m_p == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  } else {
    if (EC_POINT_copy(m_p.get(), src.m_p.get()) != 1) {
      LOG_GENERAL(WARNING, "CommitPoint copy failed");
    } else {
      m_initialized = true;
    }
  }
}

CommitPoint::~CommitPoint() {}

bool CommitPoint::Initialized() const { return m_initialized; }

unsigned int CommitPoint::Serialize(bytes& dst, unsigned int offset) const {
  // LOG_MARKER();

  if (m_initialized) {
    ECPOINTSerialize::SetNumber(dst, offset, COMMIT_POINT_SIZE, m_p);
  }

  return COMMIT_POINT_SIZE;
}

int CommitPoint::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  try {
    m_p = ECPOINTSerialize::GetNumber(src, offset, COMMIT_POINT_SIZE);
    if (m_p == nullptr) {
      LOG_GENERAL(WARNING, "Deserialization failure");
      m_initialized = false;
    } else {
      m_initialized = true;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with CommitPoint::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

void CommitPoint::Set(const CommitSecret& secret) {
  if (!secret.Initialized()) {
    LOG_GENERAL(WARNING, "Commitment secret value not initialized");
    return;
  }

  if (EC_POINT_mul(Schnorr::GetInstance().GetCurve().m_group.get(), m_p.get(),
                   secret.m_s.get(), NULL, NULL, NULL) != 1) {
    LOG_GENERAL(WARNING, "Commit gen failed");
    m_initialized = false;
  } else {
    m_initialized = true;
  }
}

CommitPoint& CommitPoint::operator=(const CommitPoint& src) {
  m_initialized = (EC_POINT_copy(m_p.get(), src.m_p.get()) == 1);
  return *this;
}

bool CommitPoint::operator==(const CommitPoint& r) const {
  unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return false;
  }

  return (m_initialized && r.m_initialized &&
          (EC_POINT_cmp(Schnorr::GetInstance().GetCurve().m_group.get(),
                        m_p.get(), r.m_p.get(), ctx.get()) == 0));
}

CommitPointHash::CommitPointHash()
    : m_h(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_h == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

CommitPointHash::CommitPointHash(const CommitPoint& point)
    : m_h(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_h == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  } else {
    Set(point);
  }
}

CommitPointHash::CommitPointHash(const bytes& src, unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init CommitPointHash.");
  }
}

CommitPointHash::CommitPointHash(const CommitPointHash& src)
    : m_h(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_h == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  } else {
    if (BN_copy(m_h.get(), src.m_h.get()) == NULL) {
      LOG_GENERAL(WARNING, "CommitPointHash copy failed");
    } else {
      m_initialized = true;
    }
  }
}

CommitPointHash::~CommitPointHash() {}

bool CommitPointHash::Initialized() const { return m_initialized; }

unsigned int CommitPointHash::Serialize(bytes& dst, unsigned int offset) const {
  // LOG_MARKER();

  if (m_initialized) {
    BIGNUMSerialize::SetNumber(dst, offset, COMMIT_POINT_HASH_SIZE, m_h);
  }

  return COMMIT_POINT_HASH_SIZE;
}

int CommitPointHash::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  try {
    m_h = BIGNUMSerialize::GetNumber(src, offset, COMMIT_POINT_HASH_SIZE);
    if (m_h == nullptr) {
      LOG_GENERAL(WARNING, "Deserialization failure");
      m_initialized = false;
    } else {
      m_initialized = true;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with CommitPointHash::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

void CommitPointHash::Set(const CommitPoint& point) {
  if (!point.Initialized()) {
    LOG_GENERAL(WARNING, "Commitment point not initialized");
    return;
  }

  m_initialized = false;
  bytes buf(Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES);

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  // The second domain separated hash function.

  // The first one is used in the Proof-of-Possession (PoP) phase.
  // PoP coincides with PoW when each node proves the knowledge
  // of the private key for a claimed public key.

  // Separation for the second hash function is defined by setting the first
  // byte to 0x01.
  sha2.Update({SECOND_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE});

  const Curve& curve = Schnorr::GetInstance().GetCurve();

  // Convert the commitment to octets first
  if (EC_POINT_point2oct(curve.m_group.get(), point.m_p.get(),
                         POINT_CONVERSION_COMPRESSED, buf.data(),
                         Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES,
                         NULL) != Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES) {
    LOG_GENERAL(WARNING, "Could not convert commitPoint to octets");
    return;
  }

  // compute H(0x01||point)
  sha2.Update(buf);
  bytes digest = sha2.Finalize();

  // Build the PointHash
  if ((BN_bin2bn(digest.data(), digest.size(), m_h.get())) == NULL) {
    LOG_GENERAL(WARNING, "Digest to scalar failed");
    return;
  }

  if (BN_nnmod(m_h.get(), m_h.get(), curve.m_order.get(), NULL) == 0) {
    LOG_GENERAL(WARNING, "Could not reduce hashpoint value modulo group order");
    return;
  }

  m_initialized = true;
}

CommitPointHash& CommitPointHash::operator=(const CommitPointHash& src) {
  m_initialized = (BN_copy(m_h.get(), src.m_h.get()) != NULL);
  return *this;
}

bool CommitPointHash::operator==(const CommitPointHash& r) const {
  return (m_initialized && r.m_initialized &&
          (BN_cmp(m_h.get(), r.m_h.get()) == 0));
}

Challenge::Challenge() : m_c(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_c == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

Challenge::Challenge(const CommitPoint& aggregatedCommit,
                     const PubKey& aggregatedPubkey, const bytes& message)
    : Challenge(aggregatedCommit, aggregatedPubkey, message, 0,
                message.size()) {}

Challenge::Challenge(const CommitPoint& aggregatedCommit,
                     const PubKey& aggregatedPubkey, const bytes& message,
                     unsigned int offset, unsigned int size)
    : m_c(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_c == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  } else {
    Set(aggregatedCommit, aggregatedPubkey, message, offset, size);
  }
}

Challenge::Challenge(const bytes& src, unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init Challenge.");
  }
}

Challenge::Challenge(const Challenge& src)
    : m_c(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_c != nullptr) {
    if (BN_copy(m_c.get(), src.m_c.get()) == NULL) {
      LOG_GENERAL(WARNING, "Challenge copy failed");
    } else {
      m_initialized = true;
    }
  } else {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

Challenge::~Challenge() {}

bool Challenge::Initialized() const { return m_initialized; }

unsigned int Challenge::Serialize(bytes& dst, unsigned int offset) const {
  // LOG_MARKER();

  if (m_initialized) {
    BIGNUMSerialize::SetNumber(dst, offset, CHALLENGE_SIZE, m_c);
  }

  return CHALLENGE_SIZE;
}

int Challenge::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  try {
    m_c = BIGNUMSerialize::GetNumber(src, offset, CHALLENGE_SIZE);
    if (m_c == nullptr) {
      LOG_GENERAL(WARNING, "Deserialization failure");
      m_initialized = false;
    } else {
      m_initialized = true;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with Challenge::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

void Challenge::Set(const CommitPoint& aggregatedCommit,
                    const PubKey& aggregatedPubkey, const bytes& message,
                    unsigned int offset, unsigned int size) {
  // Initial checks

  if (!aggregatedCommit.Initialized()) {
    LOG_GENERAL(WARNING, "Aggregated commit not initialized");
    return;
  }

  if (message.size() == 0) {
    LOG_GENERAL(WARNING, "Empty message");
    return;
  }

  if (message.size() < (offset + size)) {
    LOG_GENERAL(WARNING, "Offset and size outside message length");
    return;
  }

  // Compute the challenge c = H(r, kpub, m)

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  // The third domain separated hash function.

  // The first one is used in the Proof-of-Possession (PoP) phase.
  // PoP coincides with PoW when each node proves the knowledge
  // of the private key for a claimed public key.

  // The second one is used in the Proof-of-Possession phase.

  // Separation for the third hash function is defined by setting the first byte
  // to 0x11.
  sha2.Update({THIRD_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE});

  m_initialized = false;

  bytes buf(Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES);

  const Curve& curve = Schnorr::GetInstance().GetCurve();

  // Convert the committment to octets first
  if (EC_POINT_point2oct(curve.m_group.get(), aggregatedCommit.m_p.get(),
                         POINT_CONVERSION_COMPRESSED, buf.data(),
                         Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES,
                         NULL) != Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES) {
    LOG_GENERAL(WARNING, "Could not convert commitment to octets");
    return;
  }

  // Hash commitment
  sha2.Update(buf);

  // Clear buffer
  fill(buf.begin(), buf.end(), 0x00);

  // Convert the public key to octets
  if (EC_POINT_point2oct(curve.m_group.get(), aggregatedPubkey.m_P.get(),
                         POINT_CONVERSION_COMPRESSED, buf.data(),
                         Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES,
                         NULL) != Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES) {
    LOG_GENERAL(WARNING, "Could not convert public key to octets");
    return;
  }

  // Hash public key
  sha2.Update(buf);

  // Hash message
  sha2.Update(message, offset, size);
  bytes digest = sha2.Finalize();

  // Build the challenge
  if ((BN_bin2bn(digest.data(), digest.size(), m_c.get())) == NULL) {
    LOG_GENERAL(WARNING, "Digest to challenge failed");
    return;
  }

  if (BN_nnmod(m_c.get(), m_c.get(), curve.m_order.get(), NULL) == 0) {
    LOG_GENERAL(WARNING, "Could not reduce challenge modulo group order");
    return;
  }

  m_initialized = true;
}

Challenge& Challenge::operator=(const Challenge& src) {
  m_initialized = (BN_copy(m_c.get(), src.m_c.get()) == m_c.get());
  return *this;
}

bool Challenge::operator==(const Challenge& r) const {
  return (m_initialized && r.m_initialized &&
          (BN_cmp(m_c.get(), r.m_c.get()) == 0));
}

Response::Response() : m_r(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_r == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

Response::Response(const CommitSecret& secret, const Challenge& challenge,
                   const PrivKey& privkey)
    : m_r(BN_new(), BN_clear_free), m_initialized(false) {
  // Initial checks

  if (m_r == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  } else {
    Set(secret, challenge, privkey);
  }
}

Response::Response(const bytes& src, unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init Response.");
  }
}

Response::Response(const Response& src)
    : m_r(BN_new(), BN_clear_free), m_initialized(false) {
  if (m_r != nullptr) {
    if (BN_copy(m_r.get(), src.m_r.get()) == NULL) {
      LOG_GENERAL(WARNING, "Response copy failed");
    } else {
      m_initialized = true;
    }
  } else {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
  }
}

Response::~Response() {}

bool Response::Initialized() const { return m_initialized; }

unsigned int Response::Serialize(bytes& dst, unsigned int offset) const {
  // LOG_MARKER();

  if (m_initialized) {
    BIGNUMSerialize::SetNumber(dst, offset, RESPONSE_SIZE, m_r);
  }

  return RESPONSE_SIZE;
}

int Response::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  try {
    m_r = BIGNUMSerialize::GetNumber(src, offset, RESPONSE_SIZE);
    if (m_r == nullptr) {
      LOG_GENERAL(WARNING, "Deserialization failure");
      m_initialized = false;
    } else {
      m_initialized = true;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with Response::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

void Response::Set(const CommitSecret& secret, const Challenge& challenge,
                   const PrivKey& privkey) {
  // Initial checks

  if (m_initialized) {
    LOG_GENERAL(WARNING, "Response already initialized");
    return;
  }

  if (!secret.Initialized()) {
    LOG_GENERAL(WARNING, "Commit secret not initialized");
    return;
  }

  if (!challenge.Initialized()) {
    LOG_GENERAL(WARNING, "Challenge not initialized");
    return;
  }

  m_initialized = false;

  // Compute s = k - krpiv*c
  unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return;
  }

  const Curve& curve = Schnorr::GetInstance().GetCurve();

  // kpriv*c
  if (BN_mod_mul(m_r.get(), challenge.m_c.get(), privkey.m_d.get(),
                 curve.m_order.get(), ctx.get()) == 0) {
    LOG_GENERAL(WARNING, "BIGNUM mod mul failed");
    return;
  }

  // k-kpriv*c
  if (BN_mod_sub(m_r.get(), secret.m_s.get(), m_r.get(), curve.m_order.get(),
                 ctx.get()) == 0) {
    LOG_GENERAL(WARNING, "BIGNUM mod add failed");
    return;
  }

  m_initialized = true;
}

Response& Response::operator=(const Response& src) {
  m_initialized = (BN_copy(m_r.get(), src.m_r.get()) == m_r.get());
  return *this;
}

bool Response::operator==(const Response& r) const {
  return (m_initialized && r.m_initialized &&
          (BN_cmp(m_r.get(), r.m_r.get()) == 0));
}

MultiSig::MultiSig() {}

MultiSig::~MultiSig() {}

MultiSig& MultiSig::GetInstance() {
  static MultiSig multisig;
  return multisig;
}

shared_ptr<PubKey> MultiSig::AggregatePubKeys(const vector<PubKey>& pubkeys) {
  const Curve& curve = Schnorr::GetInstance().GetCurve();

  if (pubkeys.size() == 0) {
    LOG_GENERAL(WARNING, "Empty list of public keys");
    return nullptr;
  }

  shared_ptr<PubKey> aggregatedPubkey(new PubKey(pubkeys.at(0)));
  if (aggregatedPubkey == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return nullptr;
  }

  for (unsigned int i = 1; i < pubkeys.size(); i++) {
    if (EC_POINT_add(curve.m_group.get(), aggregatedPubkey->m_P.get(),
                     aggregatedPubkey->m_P.get(), pubkeys.at(i).m_P.get(),
                     NULL) == 0) {
      LOG_GENERAL(WARNING, "Pubkey aggregation failed");
      return nullptr;
    }
  }

  return aggregatedPubkey;
}

shared_ptr<CommitPoint> MultiSig::AggregateCommits(
    const vector<CommitPoint>& commitPoints) {
  const Curve& curve = Schnorr::GetInstance().GetCurve();

  if (commitPoints.size() == 0) {
    LOG_GENERAL(WARNING, "Empty list of commits");
    return nullptr;
  }

  shared_ptr<CommitPoint> aggregatedCommit(new CommitPoint(commitPoints.at(0)));
  if (aggregatedCommit == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return nullptr;
  }

  for (unsigned int i = 1; i < commitPoints.size(); i++) {
    if (EC_POINT_add(curve.m_group.get(), aggregatedCommit->m_p.get(),
                     aggregatedCommit->m_p.get(), commitPoints.at(i).m_p.get(),
                     NULL) == 0) {
      LOG_GENERAL(WARNING, "Commit aggregation failed");
      return nullptr;
    }
  }

  return aggregatedCommit;
}

shared_ptr<Response> MultiSig::AggregateResponses(
    const vector<Response>& responses) {
  const Curve& curve = Schnorr::GetInstance().GetCurve();

  if (responses.size() == 0) {
    LOG_GENERAL(WARNING, "Empty list of responses");
    return nullptr;
  }

  shared_ptr<Response> aggregatedResponse(new Response(responses.at(0)));
  if (aggregatedResponse == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return nullptr;
  }

  unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return nullptr;
  }

  for (unsigned int i = 1; i < responses.size(); i++) {
    if (BN_mod_add(aggregatedResponse->m_r.get(), aggregatedResponse->m_r.get(),
                   responses.at(i).m_r.get(), curve.m_order.get(),
                   ctx.get()) == 0) {
      LOG_GENERAL(WARNING, "Response aggregation failed");
      return nullptr;
    }
  }

  return aggregatedResponse;
}

shared_ptr<Signature> MultiSig::AggregateSign(
    const Challenge& challenge, const Response& aggregatedResponse) {
  if (!challenge.Initialized()) {
    LOG_GENERAL(WARNING, "Challenge not initialized");
    return nullptr;
  }

  if (!aggregatedResponse.Initialized()) {
    LOG_GENERAL(WARNING, "Response not initialized");
    return nullptr;
  }

  shared_ptr<Signature> result(new Signature());
  if (result == nullptr) {
    LOG_GENERAL(WARNING, "Memory allocation failure");
    // throw exception();
    return nullptr;
  }

  if (BN_copy(result->m_r.get(), challenge.m_c.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature generation (copy challenge) failed");
    return nullptr;
  }

  if (BN_copy(result->m_s.get(), aggregatedResponse.m_r.get()) == NULL) {
    LOG_GENERAL(WARNING, "Signature generation (copy response) failed");
    return nullptr;
  }

  return result;
}

bool MultiSig::VerifyResponse(const Response& response,
                              const Challenge& challenge, const PubKey& pubkey,
                              const CommitPoint& commitPoint) {
  try {
    // Initial checks

    if (!response.Initialized()) {
      LOG_GENERAL(WARNING, "Response not initialized");
      return false;
    }

    if (!challenge.Initialized()) {
      LOG_GENERAL(WARNING, "Challenge not initialized");
      return false;
    }

    if (!commitPoint.Initialized()) {
      LOG_GENERAL(WARNING, "Commit point not initialized");
      return false;
    }

    const Curve& curve = Schnorr::GetInstance().GetCurve();

    // The algorithm to check whether the commit point generated from its
    // resopnse is the same one received in the commit phase Check if s is in
    // [1, ..., order-1] Compute Q = sG + r*kpub return Q == commitPoint

    bool err = false;

    // Regenerate the commitmment part of the signature
    unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
        EC_POINT_new(curve.m_group.get()), EC_POINT_clear_free);
    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

    if ((ctx != nullptr) && (Q != nullptr)) {
      // 1. Check if s is in [1, ..., order-1]
      err = (BN_is_zero(response.m_r.get()) ||
             (BN_cmp(response.m_r.get(), curve.m_order.get()) != -1));
      if (err) {
        LOG_GENERAL(WARNING, "Response not in range");
        return false;
      }

      // 2. Compute Q = sG + r*kpub
      err =
          (EC_POINT_mul(curve.m_group.get(), Q.get(), response.m_r.get(),
                        pubkey.m_P.get(), challenge.m_c.get(), ctx.get()) == 0);
      if (err) {
        LOG_GENERAL(WARNING, "Commit regenerate failed");
        return false;
      }

      // 3. Q == commitPoint
      err = (EC_POINT_cmp(curve.m_group.get(), Q.get(), commitPoint.m_p.get(),
                          ctx.get()) != 0);
      if (err) {
        LOG_GENERAL(WARNING,
                    "Generated commit point doesn't match the "
                    "given one");
        return false;
      }
    } else {
      LOG_GENERAL(WARNING, "Memory allocation failure");
      // throw exception();
      return false;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with MultiSig::VerifyResponse." << ' ' << e.what());
    return false;
  }
  return true;
}

/*
 * This method is the same as:
 * bool Schnorr::Verify(const bytes& message,
 *                    const Signature& toverify, const PubKey& pubkey);
 *
 */

bool MultiSig::MultiSigVerify(const bytes& message, const Signature& toverify,
                              const PubKey& pubkey) {
  return MultiSigVerify(message, 0, message.size(), toverify, pubkey);
}

/*
 * This method is the same as:
 * Schnorr::Verify(const bytes& message, unsigned int offset,
 *                    unsigned int size, const Signature& toverify,
 *                    const PubKey& pubkey)
 * except that the underlying hash function H() is now replaced by domain
 * separated hash function H(0x11|x).
 *
 */

bool MultiSig::MultiSigVerify(const bytes& message, unsigned int offset,
                              unsigned int size, const Signature& toverify,
                              const PubKey& pubkey) {
  // This mutex is to prevent multi-threaded issues with the use of openssl
  // functions
  lock_guard<mutex> g(m_mutexMultiSigVerify);

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

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

    // The third domain separated hash function.

    // The first one is used in the Proof-of-Possession (PoP) phase.
    // PoP coincides with PoW when each node proves the knowledge
    // of the private key for a claimed public key.

    // The second one is used in CommitPointHash::Set to generate the hash of
    // the committed point.

    // Separation for the third hash function is defined by
    // setting the first byte to 0x11.
    sha2.Update({THIRD_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE});

    bytes buf(Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES);

    bool err = false;
    bool err2 = false;

    const Curve& curve = Schnorr::GetInstance().GetCurve();

    // Regenerate the commitment part of the signature
    unique_ptr<BIGNUM, void (*)(BIGNUM*)> challenge_built(BN_new(),
                                                          BN_clear_free);
    unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
        EC_POINT_new(curve.m_group.get()), EC_POINT_clear_free);
    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

    if ((challenge_built != nullptr) && (ctx != nullptr) && (Q != nullptr)) {
      // 1. Check if r,s is in [1, ..., order-1]
      err2 = (BN_is_zero(toverify.m_r.get()) ||
              BN_is_negative(toverify.m_r.get()) ||
              (BN_cmp(toverify.m_r.get(), curve.m_order.get()) != -1));
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Challenge not in range");
        return false;
      }

      err2 = (BN_is_zero(toverify.m_s.get()) ||
              BN_is_negative(toverify.m_s.get()) ||
              (BN_cmp(toverify.m_s.get(), curve.m_order.get()) != -1));
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Response not in range");
        return false;
      }

      // 2. Compute Q = sG + r*kpub
      err2 =
          (EC_POINT_mul(curve.m_group.get(), Q.get(), toverify.m_s.get(),
                        pubkey.m_P.get(), toverify.m_r.get(), ctx.get()) == 0);
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Commit regenerate failed");
        return false;
      }

      // 3. If Q = O (the neutral point), return 0;
      err2 = (EC_POINT_is_at_infinity(curve.m_group.get(), Q.get()));
      err = err || err2;
      if (err2) {
        LOG_GENERAL(WARNING, "Commit at infinity");
        return false;
      }

      // 4. r' = H(Q, kpub, m)
      // 4.1 Convert the committment to octets first
      err2 = (EC_POINT_point2oct(curve.m_group.get(), Q.get(),
                                 POINT_CONVERSION_COMPRESSED, buf.data(),
                                 Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES, NULL) !=
              Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES);
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
      err2 = (EC_POINT_point2oct(curve.m_group.get(), pubkey.m_P.get(),
                                 POINT_CONVERSION_COMPRESSED, buf.data(),
                                 Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES, NULL) !=
              Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES);
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
                       curve.m_order.get(), NULL) == 0);
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

bool MultiSig::SignKey(const bytes& messageWithPubKey, const PairOfKey& keyPair,
                       Signature& signature) {
  // This function is only used by Messenger::SetDSPoWSubmission for
  // Proof-of-Possession (PoP) phase
  return Schnorr::GetInstance().Sign(messageWithPubKey, keyPair.first,
                                     keyPair.second, signature);
}

bool MultiSig::VerifyKey(const bytes& messageWithPubKey,
                         const Signature& signature, const PubKey& pubKey) {
  // This function is only used by Messenger::GetDSPoWSubmission for
  // Proof-of-Possession (PoP) phase
  return Schnorr::GetInstance().Verify(messageWithPubKey, signature, pubKey);
}
