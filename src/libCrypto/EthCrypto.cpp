/*
 * Copyright (C) 2022 Zilliqa
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

#include "EthCrypto.h"
#include "libUtils/Logger.h"
#include "libUtils/DataConversion.h"

#include "depends/common/RLP.h"

#include "secp256k1.h"
#include "secp256k1_recovery.h"

#include <openssl/ec.h>  // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/obj_mac.h>  // for NID_secp192k1
#include <openssl/sha.h>      //for SHA512_DIGEST_LENGTH
#include <ethash/keccak.hpp>
#include <iostream>
#include <memory>
#include <string>

// Inspiration from:
// https://stackoverflow.com/questions/10906524
// https://stackoverflow.com/questions/57385412/
// https://medium.com/mycrypto/the-magic-of-digital-signatures-on-ethereum-98fe184dc9c7

// Prefix signed txs in Ethereum with Keccak256("\x19Ethereum Signed
// Message:\n32" + Keccak256(message))
constexpr uint8_t prelude[] = {25,  69,  116, 104, 101, 114, 101, 117, 109,
                               32,  83,  105, 103, 110, 101, 100, 32,  77,
                               101, 115, 115, 97,  103, 101, 58,  10,  48};

auto bnFree = [](BIGNUM* b) { BN_free(b); };
auto ecFree = [](EC_GROUP* b) { EC_GROUP_free(b); };
auto epFree = [](EC_POINT* b) { EC_POINT_free(b); };
auto esFree = [](ECDSA_SIG* b) { ECDSA_SIG_free(b); };
auto ekFree = [](EC_KEY* b) { EC_KEY_free(b); };

// https://stackoverflow.com/questions/57385412/
void SetOpensslSignature(const std::string& sSignatureInHex, ECDSA_SIG* pSign) {
  // Openssl uses raw pointers and macros for freeing but can force it into
  // smart pointers methodology
  std::unique_ptr<BIGNUM, decltype(bnFree)> rr(NULL, bnFree);
  BIGNUM* r_ptr = rr.get();
  std::unique_ptr<BIGNUM, decltype(bnFree)> ss(NULL, bnFree);
  BIGNUM* s_ptr = ss.get();

  std::string sSignatureR =
      sSignatureInHex.substr(0, sSignatureInHex.size() / 2);
  std::string sSignatureS = sSignatureInHex.substr(sSignatureInHex.size() / 2);

  BN_hex2bn(&r_ptr, sSignatureR.c_str());
  BN_hex2bn(&s_ptr, sSignatureS.c_str());

  ECDSA_SIG_set0(pSign, r_ptr, s_ptr);
}

// This function needs to set the key but also needs to decomprss the
// coordinates using EC_POINT_set_compressed_coordinates_GFp sPubKeyString
// should be hex \0 terminated string with no compression bit on it
bool SetOpensslPublicKey(const char* sPubKeyString, EC_KEY* pKey) {
  // X co-ordinate
  std::unique_ptr<BIGNUM, decltype(bnFree)> gx(NULL, bnFree);
  std::unique_ptr<BIGNUM, decltype(bnFree)> gy(NULL, bnFree);

  BIGNUM* gx_ptr = gx.get();
  BIGNUM* gy_ptr = gy.get();

  EC_KEY_set_asn1_flag(pKey, OPENSSL_EC_NAMED_CURVE);

  // From
  // https://www.oreilly.com/library/view/mastering-ethereum/9781491971932/ch04.html
  // The first byte indicates whether the y coordinate is odd or even
  int y_chooser_bit = 0;
  bool notCompressed = false;

  if (sPubKeyString[0] != '0') {
    LOG_GENERAL(WARNING,
                "Received badly set signature bit! Should be 0 and got: "
                    << sPubKeyString[0]);
    return false;
  }

  if (sPubKeyString[1] == '2') {
    y_chooser_bit = 0;
  } else if (sPubKeyString[1] == '3') {
    y_chooser_bit = 1;
  } else if (sPubKeyString[1] == '4') {
    notCompressed = true;
  } else {
    LOG_GENERAL(WARNING,
                "Received badly set signature bit! Should be 2, 3 or 4 and got: "
                    << sPubKeyString[1] << " Note: signature is: " << sPubKeyString);
  }

  // Don't want the first byte
  if (!BN_hex2bn(&gx_ptr, sPubKeyString + 2)) {
    LOG_GENERAL(WARNING, "Error getting to x binary format");
  }

  // Create a new curve group
  // Refactor: probably can have static/constexpr curve, need to check
  std::unique_ptr<EC_GROUP, decltype(ecFree)> curve_group(
      EC_GROUP_new_by_curve_name(NID_secp256k1), ecFree);

  std::unique_ptr<EC_POINT, decltype(epFree)> point(
      EC_POINT_new(curve_group.get()), epFree);

  // This performs the decompression at the same time as setting the pubKey
  if (notCompressed) {
    LOG_GENERAL(WARNING, "Not compressed - setting...");

    if (!BN_hex2bn(&gy_ptr, sPubKeyString + 2 + 64)) {
      LOG_GENERAL(WARNING, "Error getting to y binary format");
    }

    EC_POINT_set_affine_coordinates(curve_group.get(), point.get(), gx_ptr, gy_ptr, NULL);
  } else {
    EC_POINT_set_compressed_coordinates_GFp(curve_group.get(), point.get(),
                                            gx_ptr, y_chooser_bit, NULL);
  }

  if (!EC_KEY_set_public_key(pKey, point.get())) {
    LOG_GENERAL(WARNING, "ERROR! setting public key attributes");
  }

  if (EC_KEY_check_key(pKey) == 1) {
    return true;
  } else {
    LOG_GENERAL(WARNING, "ec key invalid ");
    return false;
  }
}

bool VerifyEcdsaSecp256k1(const bytes& sRandomNumber,
                          const std::string& sSignature,
                          const std::string& sDevicePubKeyInHex) {
  std::unique_ptr<ECDSA_SIG, decltype(esFree)> zSignature(ECDSA_SIG_new(),
                                                          esFree);

  SetOpensslSignature(sSignature, zSignature.get());

  std::unique_ptr<EC_KEY, decltype(ekFree)> zPublicKey(
      EC_KEY_new_by_curve_name(NID_secp256k1), ekFree);

  if (!SetOpensslPublicKey(sDevicePubKeyInHex.c_str(), zPublicKey.get())) {
    LOG_GENERAL(WARNING, "Failed to get the public key from the hex input");
  }

  auto result = ECDSA_do_verify(sRandomNumber.data(), SHA256_DIGEST_LENGTH,
                         zSignature.get(), zPublicKey.get());

  return result;
}

// Given a hex string representing the pubkey (secp256k1), return the hex
// representation of the pubkey in uncompressed format.
// The input will have the '02' prefix, and the output will have the '04' prefix
// per the 'Standards for Efficient Cryptography' specification
std::string ToUncompressedPubKey(std::string const& pubKey) {
  // Create public key pointer
  std::unique_ptr<EC_KEY, decltype(ekFree)> zPublicKey(
      EC_KEY_new_by_curve_name(NID_secp256k1), ekFree);

  // The +2 removes '0x' at the beginning of the string
  if (!SetOpensslPublicKey(pubKey.c_str() + 2, zPublicKey.get())) {
    LOG_GENERAL(WARNING,
                "Failed to get the public key from the hex input when getting "
                "uncompressed form");
  }

  // Get the size of the key
  int pubSize = i2o_ECPublicKey(zPublicKey.get(), NULL);

  if (!(pubSize == UNCOMPRESSED_SIGNATURE_SIZE)) {
    LOG_GENERAL(WARNING, "pub key to data incorrect size ");
  }

  u_int8_t pubKeyOut[UNCOMPRESSED_SIGNATURE_SIZE];
  u_int8_t* pubKeyOut2 =
      &pubKeyOut[0];  // Will end up pointing to end of memory

  if (i2o_ECPublicKey(zPublicKey.get(), &pubKeyOut2) != pubSize) {
    printf("pub key to data fail\n");
  }

  std::string ret{};

  if (pubKeyOut2 - &pubKeyOut[0] != UNCOMPRESSED_SIGNATURE_SIZE) {
    LOG_GENERAL(WARNING, "Pubkey size incorrect after decompressing:"
                             << pubKeyOut2 - &pubKeyOut[0]);
  } else {
    ret = std::string(reinterpret_cast<const char*>(pubKeyOut),
                      UNCOMPRESSED_SIGNATURE_SIZE);
  }

  return ret;
}

secp256k1_context const* getCtx()
{
  static std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
      secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY),
      &secp256k1_context_destroy
  };
  return s_ctx.get();
}

// EIP-155 : assume the chain height is high enough that the signing scheme
// is in line with EIP-155.
// message shall not contain '0x'
bytes RecoverECDSAPubSig(std::string const &message, int chain_id) {

  // First we need to parse the RSV message, then set the last three fields
  // to chain_id, 0, 0 in order to recreate what was signed
  bytes asBytes;
  int v = 0;
  bytes rs;
  DataConversion::HexStrToUint8Vec(message, asBytes);

  dev::RLP rlpStream1(asBytes);
  dev::RLPStream rlpStreamRecreated(9);

  int i = 0;

  // Iterate through the RLP message and build up what the message was before
  // it was hashed and signed. That is, same size, same fields, except
  // v = chain_id, R and S = 0
  for (const auto& item : rlpStream1) {

    auto itemBytes = item.operator bytes();

    // First 5 fields stay the same
    if (i <= 5) {
      rlpStreamRecreated << itemBytes;
    }

    // Field V
    if (i == 6) {
      rlpStreamRecreated << chain_id;
      v = uint32_t(item);
    }

    // Fields R and S
    if (i == 7 || i == 8) {
      rlpStreamRecreated << bytes{};
      rs.insert(rs.end(), itemBytes.begin(), itemBytes.end());
    }
    i++;
  }

  // Determine whether the rcid is 0,1 based on the V
  int vSelect = (v - (chain_id * 2));
  vSelect = vSelect == 35 ? 0 : vSelect;
  vSelect = vSelect == 36 ? 1 : vSelect;

  if (!(vSelect >= 0 && vSelect <= 3)) {
    LOG_GENERAL(WARNING,
                "Received badly parsed recid in raw transaction: "
                    << v << " with chainID " << chain_id << " for " << vSelect);
    return {};
  }

  auto messageRecreatedBytes = rlpStreamRecreated.out();

  // Sign original message
  auto signingHash =
      ethash::keccak256(messageRecreatedBytes.data(), messageRecreatedBytes.size());

  // Load the RS into the library
  auto* ctx = getCtx();
  secp256k1_ecdsa_recoverable_signature rawSig;
  if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rawSig,
                                                           rs.data(), vSelect)) {
    LOG_GENERAL(WARNING, "Error getting RS signature during public key reconstruction");
    return {};
  }

  // Re-create public key given signature, and message
  secp256k1_pubkey rawPubkey;
  if (!secp256k1_ecdsa_recover(ctx, &rawPubkey, &rawSig, &signingHash.bytes[0])) {
    LOG_GENERAL(WARNING, "Error recovering public key during public key reconstruction");
    return {};
  }

  // Parse the public key out of the library format
  bytes serializedPubkey(65);
  size_t serializedPubkeySize = serializedPubkey.size();
  secp256k1_ec_pubkey_serialize(
      ctx, serializedPubkey.data(), &serializedPubkeySize,
      &rawPubkey, SECP256K1_EC_UNCOMPRESSED
  );

  return serializedPubkey;
}

// nonce, gasprice, startgas, to, value, data, chainid, 0, 0
bytes GetOriginalHash(TransactionCoreInfo const &info, uint64_t chainId){

  dev::RLPStream rlpStreamRecreated(9);

  rlpStreamRecreated << info.nonce;
  rlpStreamRecreated << info.gasPrice;
  rlpStreamRecreated << info.gasLimit;
  rlpStreamRecreated << info.toAddr;
  rlpStreamRecreated << info.amount;
  rlpStreamRecreated << info.data;
  rlpStreamRecreated << chainId;
  rlpStreamRecreated << bytes{};
  rlpStreamRecreated << bytes{};

  auto signingHash =
      ethash::keccak256(rlpStreamRecreated.out().data(), rlpStreamRecreated.out().size());

  return bytes{&signingHash.bytes[0], &signingHash.bytes[32]};
}
