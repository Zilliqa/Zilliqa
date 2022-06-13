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

# pragma once

#include <iostream>      // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ec.h>      // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>   // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h> // for NID_secp192k1
#include <openssl/evp.h> //for all other OpenSSL function calls
#include <openssl/sha.h> //for SHA512_DIGEST_LENGTH
#include <ethash/keccak.hpp>

// Inspiration from:
// https://stackoverflow.com/questions/10906524

using namespace std;

constexpr int prelude[] = { 25, 69, 116, 104, 101, 114,
                           101, 117, 109, 32, 83, 105,
                           103, 110, 101, 100, 32,
                           77, 101, 115, 115, 97,
                           103, 101, 58, 10, 48 };

// https://stackoverflow.com/questions/57385412/ecdsa-do-verify-fails-to-verify-for-some-hash-only
inline bool verify_signature(const unsigned char* hash, const ECDSA_SIG* signature, EC_KEY* eckey)
{
  int verify_status = ECDSA_do_verify(hash, SHA256_DIGEST_LENGTH, signature, eckey);
  if (1 != verify_status)
  {
    printf("Failed to verify EC Signature\n");
    return false;
  }

  printf("Verifed EC Signature\n");

  return true;
}

inline void SetOpensslSignature(const std::string& sSignatureInHex, ECDSA_SIG* pSign)
{
  std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> rr(NULL, [](BIGNUM* b) { BN_free(b); });
  BIGNUM* r_ptr = rr.get();
  std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> ss(NULL, [](BIGNUM* b) { BN_free(b); });
  BIGNUM* s_ptr = ss.get();

  std::string sSignatureR = sSignatureInHex.substr(0, sSignatureInHex.size() / 2);
  std::string sSignatureS = sSignatureInHex.substr(sSignatureInHex.size() / 2);

  BN_hex2bn(&r_ptr, sSignatureR.c_str());
  BN_hex2bn(&s_ptr, sSignatureS.c_str());

  ECDSA_SIG_set0(pSign, r_ptr, s_ptr);

  return;
}

inline bool SetOpensslPublicKey(const char* sPubKeyString, EC_KEY* pKey)
{
  //const char* sPubKeyString = sPublicKeyInHex.c_str();

  char cx[65];

  std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> gx(NULL, [](BIGNUM* b) { BN_free(b); });
  std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> gy(NULL, [](BIGNUM* b) { BN_free(b); });

  BIGNUM* gx_ptr = gx.get();

  EC_KEY_set_asn1_flag(pKey, OPENSSL_EC_NAMED_CURVE);
  memcpy(cx, sPubKeyString, 64);
  cx[64] = 0;
  int y_chooser_bit = 0;

  if (!BN_hex2bn(&gx_ptr, cx)) {
    std::cout << "***** Error getting to x binary format" << std::endl;
  }

  // Create a new
  EC_GROUP *curve_group = EC_GROUP_new_by_curve_name(NID_secp256k1);
  auto point = EC_POINT_new(curve_group);

  EC_POINT_set_compressed_coordinates_GFp(curve_group, point,
                                          gx_ptr,
                                          y_chooser_bit, NULL);

  if (!EC_KEY_set_public_key(pKey, point)) {
    std::cout << "****** ERROR! setting public key attributes" << std::endl;
  }

  if (EC_KEY_check_key(pKey) == 1)
  {
    std::cout << "ec key valid " << std::endl;
    return true;
  }
  else {
    std::cout << "ec key INvalid " << std::endl;
    return false;
  }
}

inline bool Verify(const std::string& /*sRandomNumber*/, const std::string& sSignature, const std::string& sDevicePubKeyInHex)
{
  std::unique_ptr< ECDSA_SIG, std::function<void(ECDSA_SIG*)>> zSignature(ECDSA_SIG_new(), [](ECDSA_SIG* b) { ECDSA_SIG_free(b); });
  // Set up the signature...
  SetOpensslSignature(sSignature, zSignature.get());

  std::unique_ptr< EC_KEY, std::function<void(EC_KEY*)>> zPublicKey(EC_KEY_new_by_curve_name(NID_secp256k1), [](EC_KEY* b) { EC_KEY_free(b); });

  if (!SetOpensslPublicKey(sDevicePubKeyInHex.c_str()+2, zPublicKey.get())) {
    std::cout << "Failed to get the public key from the hex input" << std::endl;
  }

  uint8_t prelude[] = { 25, 69, 116, 104, 101, 114, 101, 117, 109, 32, 83, 105, 103, 110, 101, 100, 32, 77, 101, 115, 115, 97, 103, 101, 58, 10, 48 };
  auto result_prelude = ethash::keccak256(prelude, sizeof(prelude));

  return verify_signature(result_prelude.bytes, zSignature.get(), zPublicKey.get());
}

inline int VerifyEcdsaSecp256k1(std::string const &sRandomNumber, std::string const &sSignatureInHex, std::string const &sPublicKeyInHex)
{
  int ret = 0;

  if (!Verify(sRandomNumber, sSignatureInHex, sPublicKeyInHex))
    std::cout << "ECDSA Verification failed!" << std::endl;
  else {
    ret = 1;
  }

  return ret;
}

// Given a hex string representing the pubkey (secp256k1), return the hex representation of the pubkey
// in uncompressed format. The input will have the '02' prefix, and the output will have
// the '04' prefix per the 'Standards for Efficient Cryptography' specification
inline std::string toUncompressedPubKey(std::string const &pubKey){

  //std::string ret;

  // Create public key pointer
  std::unique_ptr< EC_KEY, std::function<void(EC_KEY*)>> zPublicKey(EC_KEY_new_by_curve_name(NID_secp256k1), [](EC_KEY* b) { EC_KEY_free(b); });

  if (!SetOpensslPublicKey(pubKey.c_str()+4, zPublicKey.get())) {
    std::cout << "Failed to get the public key from the hex input when getting uncompressed form" << std::endl;
  }

  // Get the size of the key
  u_int8_t pubSize = i2o_ECPublicKey(zPublicKey.get(), NULL);

  if(!(pubSize != 65)){
    cout << "pub key to data incorrect size " << pubSize << std::endl;
  }

  // pubKey = malloc(pubSize);
  u_int8_t *pubKeyOut = new u_int8_t[65];
  u_int8_t * pubKeyOut2 = pubKeyOut; // Will end up pointing to end of memory

  if(i2o_ECPublicKey(zPublicKey.get(), &pubKeyOut2) != pubSize){
    printf("pub key to data fail\n");
  }

  cout << "distance " << pubKeyOut2 - pubKeyOut << std::endl;

  std::string ret(reinterpret_cast<const char*>(pubKeyOut), 65);
  delete[] pubKeyOut;

  return ret;
}