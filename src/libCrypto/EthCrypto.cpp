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
#include <boost/algorithm/string.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy-with-user-provided-copy"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <boost/format.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "libData/AccountData/Address.h"
#include "libEth/Eth.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

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

namespace {

// Workaround for the warning/error "ignoring attributes on template argument"
// on gcc: write our own deleter for std::unique_ptr below that has no
// attributes.
void Secp256k1ContextDeleter(secp256k1_context* ctx) {
  if (ctx) secp256k1_context_destroy(ctx);
}

}  // namespace

auto bnFree = [](BIGNUM* b) { BN_free(b); };
auto ecFree = [](EC_GROUP* b) { EC_GROUP_free(b); };
auto epFree = [](EC_POINT* b) { EC_POINT_free(b); };
auto esFree = [](ECDSA_SIG* b) { ECDSA_SIG_free(b); };
auto ekFree = [](EC_KEY* b) { EC_KEY_free(b); };

bool PubKeysSame(zbytes const& pubkA, PubKey const& pubkB) {
  return PubKey(pubkA, 0) == pubkB;
}

zbytes DerivePubkey(zbytes rs, int vSelect, const unsigned char* signingHash) {
  // Load the RS into the library
  std::unique_ptr<secp256k1_context, decltype(&Secp256k1ContextDeleter)> s_ctx{
      secp256k1_context_create(SECP256K1_CONTEXT_SIGN |
                               SECP256K1_CONTEXT_VERIFY),
      &Secp256k1ContextDeleter};
  auto ctx = s_ctx.get();

  secp256k1_ecdsa_recoverable_signature rawSig;
  if (!secp256k1_ecdsa_recoverable_signature_parse_compact(
          ctx, &rawSig, rs.data(), vSelect)) {
    LOG_GENERAL(WARNING,
                "Error getting RS signature during public key reconstruction");
    return {};
  }

  // Re-create public key given signature, and message
  secp256k1_pubkey rawPubkey;
  if (!secp256k1_ecdsa_recover(ctx, &rawPubkey, &rawSig, signingHash)) {
    LOG_GENERAL(WARNING,
                "Error recovering public key during public key reconstruction");
    return {};
  }

  // Parse the public key out of the library format
  zbytes serializedPubkey(65);
  size_t serializedPubkeySize = serializedPubkey.size();
  secp256k1_ec_pubkey_serialize(ctx, serializedPubkey.data(),
                                &serializedPubkeySize, &rawPubkey,
                                SECP256K1_EC_UNCOMPRESSED);
  LOG_GENERAL(WARNING, "serialized pub key: " << DataConversion::Uint8VecToHexStrRet(serializedPubkey));

  return serializedPubkey;
}

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

  if (sPubKeyString[0] != '0') {
    LOG_GENERAL(WARNING,
                "Received badly set signature bit! Should be 0 and got: "
                    << sPubKeyString[0]);
    return false;
  }

  // From
  // https://www.oreilly.com/library/view/mastering-ethereum/9781491971932/ch04.html
  // The first byte indicates whether the y coordinate is odd or even
  int y_chooser_bit = 0;
  bool notCompressed = false;

  if (sPubKeyString[1] == '2') {
    y_chooser_bit = 0;
  } else if (sPubKeyString[1] == '3') {
    y_chooser_bit = 1;
  } else if (sPubKeyString[1] == '4') {
    notCompressed = true;
  } else {
    LOG_GENERAL(
        WARNING,
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

    EC_POINT_set_affine_coordinates(curve_group.get(), point.get(), gx_ptr,
                                    gy_ptr, NULL);
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

bool VerifyEcdsaSecp256k1(const zbytes& sRandomNumber,
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

  auto const result =
      ECDSA_do_verify(sRandomNumber.data(), SHA256_DIGEST_LENGTH,
                      zSignature.get(), zPublicKey.get());

  return result;
}

bool SignEcdsaSecp256k1(const zbytes& digest, const zbytes& privKey,
                        zbytes& signature) {
  if (digest.size() != 32) {
    LOG_GENERAL(WARNING, "Singing ECDSA: wrong digest length");
    return false;
  }

  if (privKey.size() != 32) {
    LOG_GENERAL(WARNING, "Singing ECDSA: wrong private key length");
    return false;
  }

  std::unique_ptr<secp256k1_context, decltype(&Secp256k1ContextDeleter)> s_ctx{
      secp256k1_context_create(SECP256K1_CONTEXT_SIGN |
                               SECP256K1_CONTEXT_VERIFY),
      &Secp256k1ContextDeleter};

  auto ctx = s_ctx.get();

  secp256k1_ecdsa_signature sig;

  int result =
      secp256k1_ecdsa_sign(ctx, &sig, &digest[0], &privKey[0], NULL, NULL);
  if (result != 1) {
    LOG_GENERAL(WARNING, "Failed to sign ECDSA");
    return false;
  }

  unsigned char str_signature[64];
  secp256k1_ecdsa_signature_serialize_compact(ctx, str_signature, &sig);
  signature.assign(&str_signature[0],
                   &str_signature[0] + sizeof(str_signature));

  return true;
}

// Given a hex string representing the pubkey (secp256k1), return the hex
// representation of the pubkey in uncompressed format.
// The input will have the '02' prefix, and the output will have the '04' prefix
// per the 'Standards for Efficient Cryptography' specification
zbytes ToUncompressedPubKey(std::string const& pubKey) {
  // Create public key pointer
  std::unique_ptr<EC_KEY, decltype(ekFree)> zPublicKey(
      EC_KEY_new_by_curve_name(NID_secp256k1), ekFree);

  size_t offset = 0;
  if (pubKey.size() >= 2 && pubKey[0] == '0' &&
      (pubKey[1] == 'x' || pubKey[1] == 'X')) {
    offset = 2;
  }

  // The +2 removes '0x' at the beginning of the string
  if (!SetOpensslPublicKey(pubKey.c_str() + offset, zPublicKey.get())) {
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

  zbytes ret{};

  if (pubKeyOut2 - &pubKeyOut[0] != UNCOMPRESSED_SIGNATURE_SIZE) {
    LOG_GENERAL(WARNING, "Pubkey size incorrect after decompressing:"
                             << pubKeyOut2 - &pubKeyOut[0]);
  } else {
    std::copy(&pubKeyOut[0], &pubKeyOut[UNCOMPRESSED_SIGNATURE_SIZE],
              std::back_inserter(ret));
  }

  return ret;
}

zbytes RecoverLegacyTransaction(zbytes transaction, int chain_id) {
  // First we need to parse the RSV message, then set the last three fields
  // to chain_id, 0, 0 in order to recreate what was signed
  dev::RLP rlpStream1(transaction,
                      dev::RLP::FailIfTooBig | dev::RLP::FailIfTooSmall);
  dev::RLPStream rlpStreamRecreatedBeforeEip155;
  dev::RLPStream rlpStreamRecreated(9);

  if (rlpStream1.isNull()) {
    LOG_GENERAL(WARNING, "Failed to parse raw TX RLP");
    return {};
  }

  int i = 0;
  int v = 0;
  zbytes rs;
  bool beforeEip155Tx = false;

  // Iterate through the RLP message and build up what the message was before
  // it was hashed and signed. That is, same size, same fields, except
  // v = chain_id, R and S = 0
  for (const auto& item : rlpStream1) {
    auto itemBytes = item.operator zbytes();

    // First 6 fields stay the same
    if (i < 6) {
      rlpStreamRecreated << itemBytes;
      rlpStreamRecreatedBeforeEip155 << itemBytes;
    }

    // Field V
    if (i == 6) {
      v = uint32_t(item);
      if (v == 27 || v == 28) {
        beforeEip155Tx = true;
      } else {
        rlpStreamRecreated << chain_id;
      }
    }

    // Fields R and S
    if (i == 7 || i == 8) {
      if (beforeEip155Tx == false)
        rlpStreamRecreated << zbytes{};
      zbytes b = dev::toBigEndian(dev::u256(item));
      rs.insert(rs.end(), b.begin(), b.end());
    }
    i++;
  }

  // Determine whether the rcid is 0,1 based on the V
  int vSelect = 0;
  if (beforeEip155Tx) {
    vSelect = v == 27 ? 0 : vSelect;
    vSelect = v == 28 ? 1 : vSelect;
  } else {
    vSelect = (v - (chain_id * 2));
    vSelect = vSelect == 35 ? 0 : vSelect;
    vSelect = vSelect == 36 ? 1 : vSelect;
  }

  // Chain ID of sender is a mismatch. Attempt to determine what it was
  if (!(vSelect >= 0 && vSelect <= 3)) {
    auto const clientId = -(35 - v) / 2;
    auto const clientIdAlt = -(36 - v) / 2;
    LOG_GENERAL(WARNING, "Received badly parsed recid in raw transaction: "
                             << v
                             << " . The client chain ID should match ours: "
                             << chain_id << " but it seems to be: " << clientId
                             << " or " << clientIdAlt);
    return {};
  }

  auto messageRecreatedBytes = beforeEip155Tx ? rlpStreamRecreatedBeforeEip155.out() : rlpStreamRecreated.out();

  // Sign original message
  auto signingHash = ethash::keccak256(messageRecreatedBytes.data(),
                                       messageRecreatedBytes.size());

  return DerivePubkey(rs, vSelect, &signingHash.bytes[0]);
}

zbytes RecoverEip2930Transaction(zbytes transaction, int expectedChainId) {
  dev::RLP rlpStream(transaction, dev::RLP::FailIfTooBig | dev::RLP::FailIfTooSmall);
  dev::RLPStream rlpStreamRecreated(8);

  if (rlpStream.isNull()) {
    LOG_GENERAL(WARNING, "Failed to parse raw TX RLP");
    return {};
  }

  // Parse these fields separately, since `parseEip2930Transaction` won't.
  int chainId = uint32_t(rlpStream[0]);
  int signatureYParity = uint32_t(rlpStream[8]);
  // Parse the rest of the fields into an `EthFields`.
  auto fields = Eth::parseEip2930Transaction(transaction);

  rlpStreamRecreated << chainId;
  rlpStreamRecreated << fields.nonce - 1;
  rlpStreamRecreated << fields.gasPrice;
  rlpStreamRecreated << fields.gasLimit;
  rlpStreamRecreated << fields.toAddr;
  rlpStreamRecreated << fields.amount;
  rlpStreamRecreated << fields.code;
  rlpStreamRecreated << fields.accessList;

  if (chainId != expectedChainId) {
    LOG_GENERAL(WARNING, "Chain ID mismatch: expected: " << expectedChainId << ", got: " << chainId);
  }

  auto messageRecreatedBytes = rlpStreamRecreated.out();
  // Prepend the message type specifier.
  messageRecreatedBytes.insert(messageRecreatedBytes.begin(), 0x01);

  auto hashed = ethash::keccak256(messageRecreatedBytes.data(), messageRecreatedBytes.size());

  return DerivePubkey(fields.signature, signatureYParity, &hashed.bytes[0]);
}


zbytes RecoverEip1559Transaction(zbytes transaction, int expectedChainId) {
  dev::RLP rlpStream(transaction, dev::RLP::FailIfTooBig | dev::RLP::FailIfTooSmall);
  dev::RLPStream rlpStreamRecreated(9);

  if (rlpStream.isNull()) {
    LOG_GENERAL(WARNING, "Failed to parse raw TX RLP");
    return {};
  }

  // Parse these fields separately, since `parseEip1559Transaction` won't.
  int chainId = uint32_t(rlpStream[0]);
  int signatureYParity = uint32_t(rlpStream[9]);
  // Parse the rest of the fields into an `EthFields`.
  auto fields = Eth::parseEip1559Transaction(transaction);

  rlpStreamRecreated << chainId;
  rlpStreamRecreated << fields.nonce - 1;
  rlpStreamRecreated << fields.maxPriorityFeePerGas;
  rlpStreamRecreated << fields.maxFeePerGas;
  rlpStreamRecreated << fields.gasLimit;
  rlpStreamRecreated << fields.toAddr;
  rlpStreamRecreated << fields.amount;
  rlpStreamRecreated << fields.code;
  rlpStreamRecreated << fields.accessList;

  if (chainId != expectedChainId) {
    LOG_GENERAL(WARNING, "Chain ID mismatch: expected: " << expectedChainId << ", got: " << chainId);
  }

  auto messageRecreatedBytes = rlpStreamRecreated.out();
  // Prepend the message type specifier.
  messageRecreatedBytes.insert(messageRecreatedBytes.begin(), 0x02);

  auto hashed = ethash::keccak256(messageRecreatedBytes.data(), messageRecreatedBytes.size());

  return DerivePubkey(fields.signature, signatureYParity, &hashed.bytes[0]);
}

// EIP-155 : assume the chain height is high enough that the signing scheme
// is in line with EIP-155.
// message shall not contain '0x'
zbytes RecoverECDSAPubKey(std::string const& message, int chain_id) {
  if (message.size() < 2) {
    LOG_GENERAL(WARNING, "invalid transaction. Tx: " << message);
    return {};
  }
  zbytes asBytes;
  DataConversion::HexStrToUint8Vec(message, asBytes);
  auto const firstByte = asBytes[0];
  if (firstByte == 0x01) {
    zbytes transaction(asBytes.begin() + 1, asBytes.end());
    return RecoverEip2930Transaction(transaction, chain_id);
  } else if (firstByte == 0x02) {
    zbytes transaction(asBytes.begin() + 1, asBytes.end());
    return RecoverEip1559Transaction(transaction, chain_id);
  } else if ((firstByte >= 0xc0) && (firstByte <= 0xfe)) {
    // See https://eips.ethereum.org/EIPS/eip-2718 section "Backwards Compatibility"
    return RecoverLegacyTransaction(asBytes, chain_id);
  } else {
    LOG_GENERAL(WARNING, "invalid transaction. Tx: " << message << " First byte: " << firstByte);
    return {};
  }
}

// nonce, gasprice, startgas, to, value, data, chainid, 0, 0
zbytes GetOriginalHash(TransactionCoreInfo const& info, uint64_t chainId, uint32_t v) {
  uint16_t version = DataConversion::UnpackB(info.version);
  const bool beforeEip155Tx = v == 27 || v == 28;
  switch (version) {
    case TRANSACTION_VERSION_ETH_LEGACY: {
      dev::RLPStream rlpStreamRecreated = beforeEip155Tx ? dev::RLPStream() : dev::RLPStream(9);

      rlpStreamRecreated << info.nonce - 1;
      rlpStreamRecreated << info.gasPrice;
      rlpStreamRecreated << info.gasLimit;
      zbytes toAddr;
      if (!IsNullAddress(info.toAddr)) {
        toAddr = info.toAddr.asBytes();
      }
      rlpStreamRecreated << toAddr;
      rlpStreamRecreated << info.amount;
      if (IsNullAddress(info.toAddr)) {
        rlpStreamRecreated << FromEVM(info.code);
      } else {
        rlpStreamRecreated << info.data;
      }

      if (beforeEip155Tx == false) {
        rlpStreamRecreated << chainId;
        rlpStreamRecreated << zbytes{};
        rlpStreamRecreated << zbytes{};
      }

      auto const signingHash = ethash::keccak256(rlpStreamRecreated.out().data(),
                                                rlpStreamRecreated.out().size());

      return zbytes{&signingHash.bytes[0], &signingHash.bytes[32]};
    }
    case TRANSACTION_VERSION_ETH_EIP_2930: {
      dev::RLPStream rlpStreamRecreated(8);
      rlpStreamRecreated << chainId;
      rlpStreamRecreated << info.nonce - 1;
      rlpStreamRecreated << info.gasPrice;
      rlpStreamRecreated << info.gasLimit;
      zbytes toAddr;
      if (!IsNullAddress(info.toAddr)) {
        toAddr = info.toAddr.asBytes();
      }
      rlpStreamRecreated << toAddr;
      rlpStreamRecreated << info.amount;
      if (IsNullAddress(info.toAddr)) {
        rlpStreamRecreated << FromEVM(info.code);
      } else {
        rlpStreamRecreated << info.data;
      }
      rlpStreamRecreated << info.accessList;

      auto messageRecreatedBytes = rlpStreamRecreated.out();
      // Prepend the message type specifier.
      messageRecreatedBytes.insert(messageRecreatedBytes.begin(), 0x01);

      auto hashed = ethash::keccak256(messageRecreatedBytes.data(), messageRecreatedBytes.size());
      return zbytes{&hashed.bytes[0], &hashed.bytes[32]};
    }
    case TRANSACTION_VERSION_ETH_EIP_1559: {
      dev::RLPStream rlpStreamRecreated(9);
      rlpStreamRecreated << chainId;
      rlpStreamRecreated << info.nonce - 1;
      rlpStreamRecreated << info.maxPriorityFeePerGas;
      rlpStreamRecreated << info.maxFeePerGas;
      rlpStreamRecreated << info.gasLimit;
      zbytes toAddr;
      if (!IsNullAddress(info.toAddr)) {
        toAddr = info.toAddr.asBytes();
      }
      rlpStreamRecreated << toAddr;
      rlpStreamRecreated << info.amount;
      if (IsNullAddress(info.toAddr)) {
        rlpStreamRecreated << FromEVM(info.code);
      } else {
        rlpStreamRecreated << info.data;
      }
      rlpStreamRecreated << info.accessList;

      auto messageRecreatedBytes = rlpStreamRecreated.out();
      // Prepend the message type specifier.
      messageRecreatedBytes.insert(messageRecreatedBytes.begin(), 0x02);

      auto hashed = ethash::keccak256(messageRecreatedBytes.data(), messageRecreatedBytes.size());
      return zbytes{&hashed.bytes[0], &hashed.bytes[32]};
    }
    default:
      LOG_GENERAL(WARNING, "Invalid transaction with version " << version);
      return {};
  }
}

// From a zilliqa TX, get the RLP that was sent to the node to create it
zbytes GetTransmittedRLP(TransactionCoreInfo const& info, uint64_t chainId,
                         std::string signature, uint64_t& recid, uint32_t v) {
  if (signature.size() >= 2 && signature[0] == '0' && signature[1] == 'x') {
    signature.erase(0, 2);
  }

  if (signature.size() != 128) {
    LOG_GENERAL(WARNING, "Received bad signature size: " << signature.size());
    return zbytes{};
  }

  std::string s = signature.substr(64, std::string::npos);
  signature.resize(64);

  for (int i = 0;; i++) {
    if (i >= 2) {
      LOG_GENERAL(WARNING, "Error recreating sent RLP stream");
      return zbytes{};
    }

    uint16_t version = DataConversion::UnpackB(info.version);

    switch (version) {
      case TRANSACTION_VERSION_ETH_LEGACY: {
        bool beforeEip155Tx = v == 27 || v == 28;
        dev::RLPStream rlpStreamRecreated(9);

        // Note: the nonce is decremented because of the difference between Zil and
        // Eth TXs
        rlpStreamRecreated << info.nonce - 1;
        rlpStreamRecreated << info.gasPrice;
        rlpStreamRecreated << info.gasLimit;
        zbytes toAddr;
        if (!IsNullAddress(info.toAddr)) {
          toAddr = info.toAddr.asBytes();
        }
        rlpStreamRecreated << toAddr;
        rlpStreamRecreated << info.amount;
        if (IsNullAddress(info.toAddr)) {
          rlpStreamRecreated << FromEVM(info.code);
        } else {
          rlpStreamRecreated << info.data;
        }

        if (beforeEip155Tx) {
          rlpStreamRecreated << v;
        } else {
          v = (chainId * 2) + 35 + i;
          rlpStreamRecreated << v;
        }

        rlpStreamRecreated << dev::u256("0x" + signature);
        rlpStreamRecreated << dev::u256("0x" + s);

        zbytes data = rlpStreamRecreated.out();
        auto const& asString = DataConversion::Uint8VecToHexStrRet(data);
        auto const pubK = RecoverECDSAPubKey(asString, chainId);

        if (!PubKeysSame(pubK, info.senderPubKey)) {
          continue;
        }

        recid = v;
        return data;
      }
      case TRANSACTION_VERSION_ETH_EIP_2930: {
        dev::RLPStream rlpStreamRecreated(11);
        rlpStreamRecreated << chainId;
        rlpStreamRecreated << info.nonce - 1;
        rlpStreamRecreated << info.gasPrice;
        rlpStreamRecreated << info.gasLimit;
        zbytes toAddr;
        if (!IsNullAddress(info.toAddr)) {
          toAddr = info.toAddr.asBytes();
        }
        rlpStreamRecreated << toAddr;
        rlpStreamRecreated << info.amount;
        if (IsNullAddress(info.toAddr)) {
          rlpStreamRecreated << FromEVM(info.code);
        } else {
          rlpStreamRecreated << info.data;
        }
        rlpStreamRecreated << info.accessList;
        rlpStreamRecreated << i;
        rlpStreamRecreated << dev::u256("0x" + signature);
        rlpStreamRecreated << dev::u256("0x" + s);

        zbytes data = rlpStreamRecreated.out();
        // Prepend the message type specifier.
        data.insert(data.begin(), 0x01);

        auto const& asString = DataConversion::Uint8VecToHexStrRet(data);
        auto const pubK = RecoverECDSAPubKey(asString, chainId);

        if (!PubKeysSame(pubK, info.senderPubKey)) {
          continue;
        }

        recid = i;
        return data;
      }
      case TRANSACTION_VERSION_ETH_EIP_1559: {
        dev::RLPStream rlpStreamRecreated(12);
        rlpStreamRecreated << chainId;
        rlpStreamRecreated << info.nonce - 1;
        rlpStreamRecreated << info.maxPriorityFeePerGas;
        rlpStreamRecreated << info.maxFeePerGas;
        rlpStreamRecreated << info.gasLimit;
        zbytes toAddr;
        if (!IsNullAddress(info.toAddr)) {
          toAddr = info.toAddr.asBytes();
        }
        rlpStreamRecreated << toAddr;
        rlpStreamRecreated << info.amount;
        if (IsNullAddress(info.toAddr)) {
          rlpStreamRecreated << FromEVM(info.code);
        } else {
          rlpStreamRecreated << info.data;
        }
        rlpStreamRecreated << info.accessList;
        rlpStreamRecreated << i;
        rlpStreamRecreated << dev::u256("0x" + signature);
        rlpStreamRecreated << dev::u256("0x" + s);

        zbytes data = rlpStreamRecreated.out();
        // Prepend the message type specifier.
        data.insert(data.begin(), 0x02);

        auto const& asString = DataConversion::Uint8VecToHexStrRet(data);
        auto const pubK = RecoverECDSAPubKey(asString, chainId);

        if (!PubKeysSame(pubK, info.senderPubKey)) {
          continue;
        }

        recid = i;
        return data;
      }
      default:
        LOG_GENERAL(WARNING, "Invalid transaction with version " << version);
        return {};
    }
  }
}

zbytes ToEVM(zbytes const& in) {
  if (in.empty()) {
    return in;
  }
  zbytes ret{'E', 'V', 'M'};
  std::copy(in.begin(), in.end(), std::back_inserter(ret));
  return ret;
}

zbytes FromEVM(zbytes const& in) {
  if (in.size() < 4) {
    return in;
  }
  return zbytes(in.begin() + 3, in.end());
}

zbytes StripEVM(zbytes const& in) {
  if (in.size() >= 3 && in[0] == 'E' && in[1] == 'V' && in[2] == 'M') {
    return zbytes(in.begin() + 3, in.end());
  } else {
    return in;
  }
}

zbytes CreateHash(zbytes const& rawTx) {
  auto const hash = ethash::keccak256(rawTx.data(), rawTx.size());

  zbytes hashBytes;

  hashBytes.insert(hashBytes.end(), &hash.bytes[0], &hash.bytes[32]);

  return hashBytes;
}

bool IsEthTransactionVersion(uint32_t version) {
  return version == TRANSACTION_VERSION_ETH_LEGACY || version == TRANSACTION_VERSION_ETH_EIP_2930 || version == TRANSACTION_VERSION_ETH_EIP_1559;
}

zbytes CreateContractAddr(zbytes const& senderAddr, int nonce) {
  dev::RLPStream rlpStream(2);
  rlpStream << senderAddr;
  rlpStream << nonce;

  auto const* dataPtr = rlpStream.out().data();
  auto const asBytes = zbytes(dataPtr, dataPtr + rlpStream.out().size());

  auto const hash = ethash::keccak256(asBytes.data(), asBytes.size());

  zbytes hashBytes;

  // Only the last 40 bytes needed
  hashBytes.insert(hashBytes.end(), &hash.bytes[12], &hash.bytes[32]);

  return hashBytes;
}

std::string GetR(std::string signature) {
  if (signature.size() >= 2 && signature[0] == '0' && signature[1] == 'x') {
    signature.erase(0, 2);
  }

  if (signature.size() != 128) {
    LOG_GENERAL(WARNING, "Received bad signature size: " << signature.size());
    return "";
  }

  // R is first half
  signature.resize(64);
  return "0x" + signature;
}

std::string GetS(std::string signature) {
  if (signature.size() >= 2 && signature[0] == '0' && signature[1] == 'x') {
    signature.erase(0, 2);
  }

  if (signature.size() != 128) {
    LOG_GENERAL(WARNING, "Received bad signature size: " << signature.size());
    return "";
  }

  // S is second half
  std::string s = signature.substr(64, std::string::npos);

  return "0x" + s;
}

std::string GetV(TransactionCoreInfo const& info, uint64_t chainId,
                 std::string signature) {
  uint64_t recid;

  GetTransmittedRLP(info, chainId, signature, recid);

  return (boost::format("0x%x") % recid).str();
}

// Get Address from public key, eth stye.
// The pubkeys in this database are compressed elliptic curve. Algo is:
// 1. Decompress public key
// 2. Remove first byte (compression indicator byte)
// 3. Keccak256 on remaining
// 4. Last 20 bytes is result
Address CreateAddr(zbytes const& publicKey) {
  Address address;

  // Do not hash the first byte, as it specifies the encoding
  auto result = ethash::keccak256(publicKey.data() + 1, publicKey.size() - 1);

  std::string res;
  boost::algorithm::hex(&result.bytes[12], &result.bytes[32],
                        back_inserter(res));

  // Want the last 20 bytes of the result
  std::copy(&result.bytes[12], &result.bytes[32], address.asArray().begin());

  return address;
}
