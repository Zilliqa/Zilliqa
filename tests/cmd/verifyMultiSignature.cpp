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

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <boost/algorithm/hex.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bytes = std::vector<uint8_t>;

#define PUBKEYSIZE 33
#define PUBKEY_COMPRESSED_SIZE_BYTES 33
#define SIGNATURE_CHALLENGE_SIZE 32
#define SIGNATURE_RESPONSE_SIZE 32
static const uint8_t THIRD_DOMAIN_SEPARATED_HASH_FUNCTION_BYTE = 0x11;

struct SignatureL {
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
  SHA256_CTX m_context{};
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

void StringToBytes(const std::string& in, bytes& out) {
  out.clear();
  boost::algorithm::unhex(in.begin(), in.end(), back_inserter(out));
}

SignatureL DeserializeSignature(const std::string& sig_s) {
  SignatureL sig;
  bytes sig_b;
  StringToBytes(sig_s, sig_b);

  if (sig_b.size() != SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE) {
    throw "Invalid length of signature";
  }

  BIGNUM* m_r = BN_bin2bn(sig_b.data(), SIGNATURE_CHALLENGE_SIZE, NULL);
  if (m_r == NULL) {
    throw "Cannot get m_r bignum";
  }

  BIGNUM* m_s = BN_bin2bn(sig_b.data() + SIGNATURE_CHALLENGE_SIZE,
                          SIGNATURE_RESPONSE_SIZE, NULL);
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

  std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> bn(
      BN_bin2bn(pubKey_b.data(), PUBKEYSIZE, NULL), BN_clear_free);
  if (bn == NULL) {
    throw "Cannot convert PubKey bytes to bignum";
  }

  std::unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
  if (ctx == nullptr) {
    throw "Cannot allocate memory for ctx";
  }
  EC_POINT* ecp =
      EC_POINT_bn2point(curve.m_group.get(), bn.get(), NULL, ctx.get());
  if (ecp == NULL) {
    throw "Cannot compute EC_Point";
  }
  return ecp;
}

std::shared_ptr<EC_POINT> AggregatePubKeys(
    const std::vector<std::shared_ptr<EC_POINT>>& pubkeys, const Curve& curve) {
  if (pubkeys.size() == 0) {
    throw "Empty list of public keys";
  }

  std::shared_ptr<EC_POINT> aggregatedPubkey = pubkeys[0];

  for (unsigned int i = 1; i < pubkeys.size(); i++) {
    if (EC_POINT_add(curve.m_group.get(), aggregatedPubkey.get(),
                     aggregatedPubkey.get(), pubkeys.at(i).get(), NULL) == 0) {
      throw "Pubkey aggregation failed";
    }
  }
  return aggregatedPubkey;
}

bool verifySig(const bytes& message, const SignatureL& toverify,
               const EC_POINT* pubkey, const Curve& curve) {
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
    err2 =
        (BN_is_zero(toverify.m_r.get()) || BN_is_negative(toverify.m_r.get()) ||
         (BN_cmp(toverify.m_r.get(), curve.m_order.get()) != -1));
    err = err || err2;
    if (err2) {
      throw "Challenge not in range";
    }

    err2 =
        (BN_is_zero(toverify.m_s.get()) || BN_is_negative(toverify.m_s.get()) ||
         (BN_cmp(toverify.m_s.get(), curve.m_order.get()) != -1));
    err = err || err2;
    if (err2) {
      throw "Response not in range";
    }

    // 2. Compute Q = sG + r*kpub
    err2 = (EC_POINT_mul(curve.m_group.get(), Q.get(), toverify.m_s.get(),
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
                               PUBKEY_COMPRESSED_SIZE_BYTES,
                               NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
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
                               PUBKEY_COMPRESSED_SIZE_BYTES,
                               NULL) != PUBKEY_COMPRESSED_SIZE_BYTES);
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
                     curve.m_order.get(), ctx.get()) == 0);
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

namespace po = boost::program_options;

int main(int argc, char** argv) {
  po::options_description desc("Options");
  std::string message;
  std::string pubk_fn;
  std::string signature;
  Curve curve;

  desc.add_options()("help,h", "Print help messages")(
      "message,m", po::value<std::string>(&message)->required(),
      "Message string in hexadecimal format")(
      "signature,s", po::value<std::string>(&signature)->required(),
      "Filename containing private keys each per line")(
      "pubk,u", po::value<std::string>(&pubk_fn)->required(),
      "Filename containing public keys each per line");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
      return -1;
    }
    po::notify(vm);
  } catch (boost::program_options::required_option& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cout << desc;
    return -1;
  } catch (boost::program_options::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    return -1;
  }

  bytes msg(message.begin(), message.end());

  std::vector<std::shared_ptr<EC_POINT>> pubKeys;
  std::string line;
  std::fstream pubFile(pubk_fn, std::ios::in);
  try {
    while (getline(pubFile, line)) {
      pubKeys.push_back(std::shared_ptr<EC_POINT>(
          DeserializePubKey(line, curve), EC_POINT_clear_free));
    }
  } catch (std::exception& e) {
    std::cerr << "Problem occured when processing public keys on line: "
              << pubKeys.size() + 1 << std::endl;
    return -1;
  }

  try {
    std::shared_ptr<EC_POINT> aggregated_pk = AggregatePubKeys(pubKeys, curve);

    SignatureL sig = DeserializeSignature(signature);

    if (verifySig(msg, sig, aggregated_pk.get(), curve))
      return 0;
    else
      return 1;
  } catch (const char* msg) {
    std::cerr << msg << std::endl;
    return -1;
  }
}
