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

#ifndef ZILLIQA_SRC_LIBCRYPTO_ETHCRYPTO_H_
#define ZILLIQA_SRC_LIBCRYPTO_ETHCRYPTO_H_

#include <openssl/ecdsa.h>  // for ECDSA_do_sign, ECDSA_do_verify
#include "common/BaseType.h"
#include <Schnorr.h>

#include <string>

constexpr unsigned int UNCOMPRESSED_SIGNATURE_SIZE = 65;

bool VerifyEcdsaSecp256k1(const std::string& sRandomNumber,
                          const std::string& sSignature,
                          const std::string& sDevicePubKeyInHex);

// Given a hex string representing the pubkey (secp256k1), return the hex
// representation of the pubkey in uncompressed format.
// The input will have the '02' prefix, and the output will have the '04' prefix
// per the 'Standards for Efficient Cryptography' specification
std::string ToUncompressedPubKey(const std::string& pubKey);

bytes recoverECDSAPubSig(std::string const& message, int chain_id);

struct EthFields {
  uint32_t version{};
  uint64_t nonce{};  // counter: the number of tx from m_fromAddr
  bytes toAddr;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit{};
  bytes code;
  bytes data;
  bytes signature;
};

EthFields parseRawTxFields(std::string const& message);

PubKey toPubKey(bytes const& key);

#endif  // ZILLIQA_SRC_LIBCRYPTO_ETHCRYPTO_H_
