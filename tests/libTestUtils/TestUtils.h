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

#ifndef __TESTUTILS_H__
#define __TESTUTILS_H__

#include <Schnorr.h>
#include <limits>
#include <random>
#include <tuple>
#include "common/BaseType.h"
#include "libBlockchain/BlockBase.h"
#include "libBlockchain/BlockHashSet.h"
#include "libBlockchain/DSBlockHeader.h"
#include "libBlockchain/MicroBlockHeader.h"
#include "libBlockchain/TxBlockHeader.h"
#include "libBlockchain/VCBlockHeader.h"
#include "libData/AccountData/Transaction.h"
#include "libNetwork/Peer.h"

namespace TestUtils {
inline void Initialize() {}

// Data Generation Functions

template <typename T>
T RandomIntInRng(T n, T m) {
  thread_local std::mt19937 rng;
  thread_local bool seeded = false;
  if (!seeded) {
    rng.seed(std::random_device()());
    seeded = true;
  }
  return std::uniform_int_distribution<T>{n, m}(rng);
}

uint8_t Dist1to99();
uint8_t DistUint8();
uint16_t DistUint16();
uint32_t DistUint32();
uint64_t DistUint64();
uint128_t DistUint128();
uint256_t DistUint256();

PubKey GenerateRandomPubKey();
PubKey GenerateRandomPubKey(const PrivKey&);
PairOfKey GenerateRandomKeyPair();
Peer GenerateRandomPeer();
Peer GenerateRandomPeer(uint8_t, bool);
DSBlockHeader GenerateRandomDSBlockHeader();
MicroBlockHeader GenerateRandomMicroBlockHeader();
TxBlockHeader GenerateRandomTxBlockHeader();
VCBlockHeader GenerateRandomVCBlockHeader();
DSBlockHeader createDSBlockHeader(const uint64_t&);
TxBlockHeader createTxBlockHeader(const uint64_t&);
CoSignatures GenerateRandomCoSignatures();
Signature GetSignature(const zbytes&, const PairOfKey&);
Signature GenerateRandomSignature();

Transaction GenerateRandomTransaction(const unsigned int version,
                                      const uint64_t& nonce,
                                      const Transaction::ContractType& type);

DequeOfNode GenerateRandomDSCommittee(uint32_t);

std::vector<bool> GenerateRandomBooleanVector(size_t);

Shard GenerateRandomShard(size_t);
DequeOfShard GenerateDequeueOfShard(size_t);
std::string GenerateRandomString(size_t);
zbytes GenerateRandomCharVector(size_t);
}  // namespace TestUtils

#endif  // __TESTUTILS_H__
