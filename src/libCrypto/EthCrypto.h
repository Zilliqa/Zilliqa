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

bool SignEcdsaSecp256k1(const bytes& digest, const bytes& privKey,
                        bytes& signature);

bool VerifyEcdsaSecp256k1(const bytes& sRandomNumber,
                          const std::string& sSignature,
                          const std::string& sDevicePubKeyInHex);

// Given a hex string representing the pubkey (secp256k1), return the hex
// representation of the pubkey in uncompressed format.
// The input will have the '02' prefix, and the output will have the '04' prefix
// per the 'Standards for Efficient Cryptography' specification
std::string ToUncompressedPubKey(const std::string& pubKey);

// Recover the public signature of a transaction given its RLP
bytes RecoverECDSAPubKey(std::string const& message, int chain_id);

// Get the hash that was signed in order to create the transaction signature.
// Note this is different from the transaction hash
bytes GetOriginalHash(TransactionCoreInfo const& info, uint64_t chainId);

// Given a native transaction, get the corresponding RLP (that was sent to
// create it)
std::string GetTransmittedRLP(TransactionCoreInfo const& info, uint64_t chainId,
                              std::string signature, uint64_t& recid);

// As a workaround, code/data strings have an evm prefix to distinguish them,
// but this must be stripped before it goes to the EVM
bytes ToEVM(bytes const& in);
bytes FromEVM(bytes const& in);
bytes StripEVM(bytes const& in);

// Create an ethereum style transaction hash
bytes CreateHash(std::string const& rawTx);

// Create the eth-style contract address given the sender address and nonce
bytes CreateContractAddr(bytes const& senderAddr, int nonce);

// Given an ethereum public key, derive the address
bytes CreateEthAddrFromPubkey(bytes const& pubKey);

std::string GetR(std::string signature);
std::string GetS(std::string signature);
std::string GetV(TransactionCoreInfo const& info, uint64_t chainId,
                 std::string signature);


// todo : comment
Address GetAddressFromPublicKeyEthX(bytes const& publicKey);

#endif  // ZILLIQA_SRC_LIBCRYPTO_ETHCRYPTO_H_
