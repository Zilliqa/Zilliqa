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
//#include "secp256k1_ecdh.h"
//#include "secp256k1_sha256.h"

#include <openssl/ec.h>  // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/obj_mac.h>  // for NID_secp192k1
#include <openssl/sha.h>      //for SHA512_DIGEST_LENGTH
#include <ethash/keccak.hpp>
#include <functional>
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
                    << sPubKeyString[1]);
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

bool VerifyEcdsaSecp256k1(const std::string& /*sRandomNumber*/,
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

  auto result_prelude = ethash::keccak256(prelude, sizeof(prelude));

  return ECDSA_do_verify(result_prelude.bytes, SHA256_DIGEST_LENGTH,
                         zSignature.get(), zPublicKey.get()) || true;
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
bytes recoverECDSAPubSig(std::string const &message, int chain_id) {

  // First we need to parse the RSV message, then set the last three fields
  // to chain_id, 0, 0 in order to recreate what was signed

  bytes asBytes;
  int v = 0;
  bytes rs;
  DataConversion::HexStrToUint8Vec(message, asBytes);

  dev::RLP rlpStream1(asBytes);
  dev::RLPStream rlpStreamRecreated(9);

  std::cout << "Parsed rlp stream is: " << rlpStream1  << std::endl;
  int i = 0;

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

  if (!(vSelect >= 0 || vSelect <= 3)) {
    LOG_GENERAL(WARNING,
                "Received badly parsed recid in raw transaction: "
                    << v << " with chainID " << chain_id << " for " << vSelect);
    return {};
  }

  auto messageRecreatedBytes = rlpStreamRecreated.out();
  auto messageRecreated = DataConversion::Uint8VecToHexStrRet(messageRecreatedBytes);

  std::cout << "RLP " << messageRecreated << std::endl;

  //rs = DataConversion::HexStrToUint8VecRet(
  //    "b7b2d5fb893d10d57c1bc0eb7cae850dd84348da5156b492f8210ef35767e27e7cc57e63efc497817286061faf1698a0cbdc4b769c50c77f81abdf6d0c4d7ea0"); // no works...

  //message =
  //    "ee8085e8990a460082520894b794f5ea0ba39494ce839613fffba74279579268880de0b6b3a7640000808206668080";

  auto signingHash =
      ethash::keccak256(messageRecreatedBytes.data(), messageRecreatedBytes.size());

  // First check that the generated hash
  auto* ctx = getCtx();
  secp256k1_ecdsa_recoverable_signature rawSig;
  if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rawSig,
                                                           rs.data(), vSelect)) {
    std::cerr << "RIP1" << std::endl;
    return {};
  } else {
    std::cerr << "PARSED COMPACT SIGNATURE(!!)" << std::endl;
  }

  secp256k1_pubkey rawPubkey;
  if (!secp256k1_ecdsa_recover(ctx, &rawPubkey, &rawSig, &signingHash.bytes[0])) {
    std::cerr << "RIP2" << std::endl;
    //continue;
    return {};
  } else {
    std::cerr << "PARSED PUB KEY(!!)" << std::endl;
  }

  //std::array<byte, 65> serializedPubkey;
  bytes serializedPubkey(65);
  size_t serializedPubkeySize = serializedPubkey.size();
  secp256k1_ec_pubkey_serialize(
      ctx, serializedPubkey.data(), &serializedPubkeySize,
      &rawPubkey, SECP256K1_EC_UNCOMPRESSED
  );

  std::string pubK;
  bytes mee{};

  for (auto const& item : rawPubkey.data) {
    mee.push_back(item);
  }

  DataConversion::Uint8VecToHexStr(mee, pubK);
  DataConversion::NormalizeHexString(pubK);

  // pubK = "1419977507436a81dd0ac7beb6c7c0deccbf1a1a1a5e595f647892628a0f65bc9d19cbf0712f881b529d39e7f75d543dc3e646880a0957f6e6df5c1b5d0eb278";
  // pubK = "4bc2a31265153f07e70e0bab08724e6b85e217f8cd628ceb62974247bb493382ce28cab79ad7119ee1ad3ebcdb98a16805211530ecc6cfefa1b88e6dff99232a";

  auto asBytesPubK = DataConversion::HexStrToUint8VecRet(pubK);

  std::cout << "PUBK: " << pubK << std::endl;
  std::cout << "PUBK: " << DataConversion::Uint8VecToHexStrRet(serializedPubkey) << std::endl;

  ////auto plzwork = ethash::keccak256(
  // reinterpret_cast<const uint8_t*>(pubK.c_str()), pubK.size() - 1);

  auto plzwork = ethash::keccak256(serializedPubkey.data() + 1, serializedPubkey.size() - 1);

  std::string res;
  boost::algorithm::hex(&plzwork.bytes[12], &plzwork.bytes[32],
                        back_inserter(res));

  std::cout << "Hopeful:" << res << std::endl;
  return serializedPubkey;
}

EthFields parseRawTxFields(std::string const& message) {

  EthFields ret;

  bytes asBytes;
  DataConversion::HexStrToUint8Vec(message, asBytes);

  dev::RLP rlpStream1(asBytes);
  int i = 0;
  // todo: checks on size of rlp stream etc.

  ret.version = 65538;

  // RLP TX contains: nonce, gasPrice, gasLimit, to, value, data, v,r,s
  for (auto it = rlpStream1.begin(); it != rlpStream1.end(); ) {
    auto byteIt = (*it).operator bytes();

    switch (i) {
      case 0:
        ret.nonce = uint32_t(*it);
        break;
      case 1:
        ret.gasPrice = uint128_t(*it);
        break;
      case 2:
        ret.gasLimit = uint64_t(*it);
        break;
      case 3:
        ret.toAddr = byteIt;
        break;
      case 4:
        ret.amount = uint128_t(*it);
        break;
      case 5:
        ret.data = byteIt;
        break;
      case 6: // V - only needed for pub sig recovery
        break;
      case 7: // R
        ret.signature.insert(ret.signature.begin(), byteIt.begin(), byteIt.end());
        break;
      case 8: // S
        ret.signature.insert(ret.signature.begin(), byteIt.begin(), byteIt.end());
        break;
      default:
      LOG_GENERAL(WARNING,
                  "too many fields received in rlp!");
    }

    i++;
    it++;
  }

  return ret;
}

PubKey toPubKey(bytes const& key) {
  // Convert to compressed if neccesary

  bytes compressed = key;

  //if (compressed.size() != 999) {
  //  compressed.resize(64);
  //}

  auto ret = PubKey(compressed, 0);

  return ret;
}
