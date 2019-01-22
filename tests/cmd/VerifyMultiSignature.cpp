/*
 * test_signmultisig.cpp
 *
 *  Created on: Jan 20, 2019
 *      Author: jenda
 */

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <boost/algorithm/hex.hpp>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <memory>

using bytes = std::vector<uint8_t>;

#define PUBKEYSIZE 33
#define PUBKEY_COMPRESSED_SIZE_BYTES 33
#define SIGNATURE_CHALLENGE_SIZE 32
#define SIGNATURE_RESPONSE_SIZE 32
static const uint8_t THIRD_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE = 0x11;

struct Signature {
  std::shared_ptr<BIGNUM> m_r;
  std::shared_ptr<BIGNUM> m_s;
};

struct Curve {

  /// EC group
  std::shared_ptr<EC_GROUP> m_group;

  /// Order of the group.
  std::shared_ptr<BIGNUM> m_order;

  Curve()
      : m_group(EC_GROUP_new_by_curve_name(NID_secp256k1), EC_GROUP_clear_free),
        m_order(BN_new(), BN_clear_free) {
    if (m_order == nullptr) {
      throw "Curve order setup failed";
    }

    if (m_group == nullptr) {
      throw "Curve group setup failed";
    }
    // Get group order
    if (!EC_GROUP_get_order(m_group.get(), m_order.get(), NULL)) {
      throw "Recover curve order failed";
    }
  }
};

#define HASH_VARIANT_256 256

template <unsigned int SIZE>
class SHA2 {
  static const unsigned int HASH_OUTPUT_SIZE = SIZE / 8;
  SHA256_CTX m_context;
  bytes output;

 public:
  /// Constructor.
  SHA2() : output(HASH_OUTPUT_SIZE) {
    if (SIZE != HASH_VARIANT_256) {
      throw "Cannot handle such a hash variant";
    }

    Reset();
  }

  /// Destructor.
  ~SHA2() {}

  /// Hash update function.
  void Update(const bytes& input) {
    if (input.size() == 0) {
      return;
    }

    SHA256_Update(&m_context, input.data(), input.size());
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

void StringToBytes(const std::string in, bytes& out) {
  out.clear();
  boost::algorithm::unhex(in.begin(), in.end(),
                          back_inserter(out));
}

Signature DeserializeSignature(const std::string sig_s) {
  Signature sig;
  bytes sig_b;
  StringToBytes(sig_s, sig_b);

  if (sig_b.size() != SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE) {
    throw "Invalid length of signature";
  }

  BIGNUM* m_r = BN_bin2bn(sig_b.data(), SIGNATURE_CHALLENGE_SIZE, NULL);
  if (m_r == NULL) {
    throw "Cannot get m_r bignum";
  }

  BIGNUM* m_s = BN_bin2bn(sig_b.data() + SIGNATURE_CHALLENGE_SIZE, SIGNATURE_RESPONSE_SIZE, NULL);
  if (m_s == NULL) {
    throw "Cannot get m_s bignum";
  }

  sig.m_r = std::shared_ptr<BIGNUM>(m_r, BN_clear_free);
  sig.m_s = std::shared_ptr<BIGNUM>(m_s, BN_clear_free);

  return sig;
}

EC_POINT* DeserializePubKey(const std::string& pubKey_s, Curve& curve) {

  bytes pubKey_b;

  StringToBytes(pubKey_s, pubKey_b);
  if (pubKey_b.size() == 0) {
    return nullptr;
  }

  std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> bn(BN_bin2bn(pubKey_b.data(), PUBKEYSIZE, NULL), BN_clear_free);
  if (bn == NULL) {
    throw "Cannot convert PubKey bytes to bignum";
  }

  std::unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    throw "Cannot allocate memory for ctx";
  }
  EC_POINT* ecp = EC_POINT_bn2point(curve.m_group.get(), bn.get(), NULL, ctx.get());
  if (ecp == NULL) {
    throw "Cannot compute EC_Point";
  }
  return ecp;
}

bool verifySig(const bytes& message, const Signature& toverify, const EC_POINT* pubkey, Curve& curve){

  // Main verification procedure

  // The algorithm to check the signature (r, s) on a message m using a public
  // key kpub is as follows
  // 1. Check if r,s is in [1, ..., order-1]
  // 2. Compute Q = sG + r*kpub
  // 3. If Q = O (the neutral point), return 0;
  // 4. r' = H(Q, kpub, m)
  // 5. return r' == r

//  bytes buf(PUBKEY_COMPRESSED_SIZE_BYTES);
//  SHA2<HASH_VARIANT_256> sha2;
//
//  bool err = false;
//  bool err2 = false;
//
//  // Regenerate the commitmment part of the signature
//  std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> challenge_built(BN_new(),
//                                                        BN_clear_free);
//  std::unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
//      EC_POINT_new(curve.m_group.get()), EC_POINT_clear_free);
//  std::unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
//
//  if ((challenge_built != nullptr) && (ctx != nullptr) && (Q != nullptr)) {
//    // 1. Check if r,s is in [1, ..., order-1]
//    err2 = (BN_is_zero(toverify.m_r.get()) ||
//            BN_is_negative(toverify.m_r.get()) ||
//            (BN_cmp(toverify.m_r.get(), curve.m_order.get()) != -1));
//    err = err || err2;
//    if (err2) {
//      throw "Challenge not in range";
//    }
//
//    err2 = (BN_is_zero(toverify.m_s.get()) ||
//            BN_is_negative(toverify.m_s.get()) ||
//            (BN_cmp(toverify.m_s.get(), curve.m_order.get()) != -1));
//    err = err || err2;
//    if (err2) {
//      throw "Response not in range";
//    }
//
//    // 2. Compute Q = sG + r*kpub
//    err2 =
//        (EC_POINT_mul(curve.m_group.get(), Q.get(), toverify.m_s.get(),
//            pubkey, toverify.m_r.get(), ctx.get()) == 0);
//    err = err || err2;
//    if (err2) {
//      throw "Commit regenerate failed";
//    }
//
//    // 3. If Q = O (the neutral point), return 0;
//    err2 = (EC_POINT_is_at_infinity(curve.m_group.get(), Q.get()));
//    err = err || err2;
//    if (err2) {
//      throw "Commit at infinity";
//    }
//
//    // 4. r' = H(Q, kpub, m)
//    // 4.1 Convert the committment to octets first
//    err2 = (EC_POINT_point2oct(curve.m_group.get(), Q.get(),
//                               POINT_CONVERSION_COMPRESSED, buf.data(),
//                               PUBKEY_COMPRESSED_SIZE_BYTES,
//                               NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
//    err = err || err2;
//    if (err2) {
//      throw "Commit octet conversion failed";
//    }
//
//    // Hash commitment
//    sha2.Update(buf);
//
//    // Reset buf
//    fill(buf.begin(), buf.end(), 0x00);
//
//    // 4.2 Convert the public key to octets
//    err2 = (EC_POINT_point2oct(curve.m_group.get(), pubkey,
//                               POINT_CONVERSION_COMPRESSED, buf.data(),
//                               PUBKEY_COMPRESSED_SIZE_BYTES,
//                               NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
//    err = err || err2;
//    if (err2) {
//      throw "Pubkey octet conversion failed";
//    }
//
//    // Hash public key
//    sha2.Update(buf);
//
//    // 4.3 Hash message
//    sha2.Update(message);
//    bytes digest = sha2.Finalize();
//
//    // 5. return r' == r
//    err2 = (BN_bin2bn(digest.data(), digest.size(), challenge_built.get()) ==
//            NULL);
//    err = err || err2;
//    if (err2) {
//      throw "Challenge bin2bn conversion failed";
//    }
//
//    err2 = (BN_nnmod(challenge_built.get(), challenge_built.get(),
//                     curve.m_order.get(), NULL) == 0);
//    err = err || err2;
//    if (err2) {
//      throw "Challenge rebuild mod failed";
//    }
//
//    sha2.Reset();
//  } else {
//    throw "Memory allocation failure";
//  }
//  return BN_cmp(challenge_built.get(), toverify.m_r.get()) == 0;




  // Main verification procedure

  // The algorithm to check the signature (r, s) on a message m using a public
  // key kpub is as follows
  // 1. Check if r,s is in [1, ..., order-1]
  // 2. Compute Q = sG + r*kpub
  // 3. If Q = O (the neutral point), return 0;
  // 4. r' = H(Q, kpub, m)
  // 5. return r' == r

  SHA2<HASH_VARIANT_256> sha2;

  // The third domain separated hash function.

  // The first one is used in the Proof-of-Possession (PoP) phase.
  // PoP coincides with PoW when each node proves the knowledge
  // of the private key for a claimed public key.

  // The second one is used in CommitPointHash::Set to generate the hash of
  // the committed point.

  // Separation for the third hash function is defined by
  // setting the first byte to 0x11.
  sha2.Update({THIRD_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE});

  bytes buf(PUBKEY_COMPRESSED_SIZE_BYTES);

  bool err = false;
  bool err2 = false;

  // Regenerate the commitment part of the signature
  std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> challenge_built(BN_new(),
                                                        BN_clear_free);
  std::unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
      EC_POINT_new(curve.m_group.get()), EC_POINT_clear_free);
  std::unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

  if ((challenge_built != nullptr) && (ctx != nullptr) && (Q != nullptr)) {
    // 1. Check if r,s is in [1, ..., order-1]
    err2 = (BN_is_zero(toverify.m_r.get()) ||
            BN_is_negative(toverify.m_r.get()) ||
            (BN_cmp(toverify.m_r.get(), curve.m_order.get()) != -1));
    err = err || err2;
    if (err2) {
      throw "Challenge not in range";
    }

    err2 = (BN_is_zero(toverify.m_s.get()) ||
            BN_is_negative(toverify.m_s.get()) ||
            (BN_cmp(toverify.m_s.get(), curve.m_order.get()) != -1));
    err = err || err2;
    if (err2) {
      throw "Response not in range";
    }

    // 2. Compute Q = sG + r*kpub
    err2 =
        (EC_POINT_mul(curve.m_group.get(), Q.get(), toverify.m_s.get(),
                      pubkey, toverify.m_r.get(), ctx.get()) == 0);
    err = err || err2;
    if (err2) {
      throw "Commit regenerate failed";
    }

    // 3. If Q = O (the neutral point), return 0;
    err2 = (EC_POINT_is_at_infinity(curve.m_group.get(), Q.get()));
    err = err || err2;
    if (err2) {
      throw "Commit at infinity";
    }

    // 4. r' = H(Q, kpub, m)
    // 4.1 Convert the committment to octets first
    err2 = (EC_POINT_point2oct(curve.m_group.get(), Q.get(),
                               POINT_CONVERSION_COMPRESSED, buf.data(),
                               PUBKEY_COMPRESSED_SIZE_BYTES, NULL) !=
            PUBKEY_COMPRESSED_SIZE_BYTES);
    err = err || err2;
    if (err2) {
      throw "Commit octet conversion failed";
    }

    // Hash commitment
    sha2.Update(buf);

    // Reset buf
    fill(buf.begin(), buf.end(), 0x00);

    // 4.2 Convert the public key to octets
    err2 = (EC_POINT_point2oct(curve.m_group.get(), pubkey,
                               POINT_CONVERSION_COMPRESSED, buf.data(),
                               PUBKEY_COMPRESSED_SIZE_BYTES, NULL) !=
            PUBKEY_COMPRESSED_SIZE_BYTES);
    err = err || err2;
    if (err2) {
      throw "Pubkey octet conversion failed";

    }

    // Hash public key
    sha2.Update(buf);

    // 4.3 Hash message
    sha2.Update(message);
    bytes digest = sha2.Finalize();

    // 5. return r' == r
    err2 = (BN_bin2bn(digest.data(), digest.size(), challenge_built.get()) ==
            NULL);
    err = err || err2;
    if (err2) {
      throw "Challenge bin2bn conversion failed";
    }

    err2 = (BN_nnmod(challenge_built.get(), challenge_built.get(),
                     curve.m_order.get(), NULL) == 0);
    err = err || err2;
    if (err2) {
      throw "Challenge rebuild mod failed";
    }

    sha2.Reset();
  } else {
    throw "Memory allocation failure";
    return false;
  }
  return (!err) && (BN_cmp(challenge_built.get(), toverify.m_r.get()) == 0);
}



int main(int argc, char** argv) {
  (void) argc;
  (void) argv;

  std::string pk = argv[1];
  std::string signature = argv[2];
  std::string message = argv[3];
  bytes msg;
  Curve curve;
  try {
    EC_POINT* pubKey = DeserializePubKey(pk, curve);
    Signature sig = DeserializeSignature(signature);
    StringToBytes(message, msg);
    bool v = verifySig(msg, sig, pubKey, curve);
    std::cout << "verify " << v << std::endl;
  }
  catch (const char* msg) {
     std::cerr << msg << std::endl;
   }
}



