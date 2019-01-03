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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <limits>
#include <random>
#include <tuple>
#include "common/BaseType.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/DSBlockHeader.h"
#include "libData/BlockData/BlockHeader/MicroBlockHeader.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Peer.h"

static std::mt19937 rng;

namespace TestUtils {
void Initialize();

// Data Generation Functions

template <typename T>
T RandomIntInRng(T n, T m) {
  return std::uniform_int_distribution<T>{n, m}(rng);
}

uint8_t Dist1to99();
uint8_t DistUint8();
uint16_t DistUint16();
uint32_t DistUint32();
uint64_t DistUint64();
boost::multiprecision::uint128_t DistUint128();
boost::multiprecision::uint256_t DistUint256();

PubKey GenerateRandomPubKey();
PubKey GenerateRandomPubKey(PrivKey);
KeyPair GenerateRandomKeyPair();
Peer GenerateRandomPeer();
Peer GenerateRandomPeer(uint8_t, bool);
DSBlockHeader GenerateRandomDSBlockHeader();
MicroBlockHeader GenerateRandomMicroBlockHeader();
TxBlockHeader GenerateRandomTxBlockHeader();
VCBlockHeader GenerateRandomVCBlockHeader();
FallbackBlockHeader GenerateRandomFallbackBlockHeader();
CoSignatures GenerateRandomCoSignatures();
Signature GetSignature(const bytes&, const PrivKey&, const PubKey&);
Signature GenerateRandomSignature();

using DS_Comitte_t = std::deque<std::pair<PubKey, Peer>>;
DS_Comitte_t GenerateRandomDSCommittee(uint32_t);

Shard GenerateRandomShard(size_t);
DequeOfShard GenerateDequeueOfShard(size_t);
std::string GenerateRandomString(size_t);
bytes GenerateRandomCharVector(size_t);
}  // namespace TestUtils

#endif  // __TESTUTILS_H__
