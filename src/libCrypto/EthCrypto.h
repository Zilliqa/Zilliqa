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

#include <Schnorr.h>
#include <openssl/ecdsa.h>  // for ECDSA_do_sign, ECDSA_do_verify
#include <string>
#include "common/BaseType.h"
#include "libData/AccountData/Transaction.h"

constexpr unsigned int UNCOMPRESSED_SIGNATURE_SIZE = 65;

bool VerifyEcdsaSecp256k1(const bytes& sRandomNumber,
                          const std::string& sSignature,
                          const std::string& sDevicePubKeyInHex);

// Given a hex string representing the pubkey (secp256k1), return the hex
// representation of the pubkey in uncompressed format.
// The input will have the '02' prefix, and the output will have the '04' prefix
// per the 'Standards for Efficient Cryptography' specification
std::string ToUncompressedPubKey(const std::string& pubKey);

bytes RecoverECDSAPubSig(std::string const& message, int chain_id);

bytes GetOriginalHash(TransactionCoreInfo const& info, uint64_t chainId);
std::string GetTransmittedRLP(TransactionCoreInfo const& info, uint64_t chainId, std::string signature);

bytes ToEVM(bytes const& in);
bytes FromEVM(bytes const& in);
bytes StripEVM(bytes const& in);

std::string CreateHash(std::string const& rawTx);

#endif  // ZILLIQA_SRC_LIBCRYPTO_ETHCRYPTO_H_