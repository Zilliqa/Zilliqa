/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
Address GetAddressFromPubKey(PubKey);
Peer GenerateRandomPeer();
Peer GenerateRandomPeer(uint8_t, bool);
DSBlockHeader GenerateRandomDSBlockHeader();
MicroBlockHeader GenerateRandomMicroBlockHeader();
TxBlockHeader GenerateRandomTxBlockHeader();
VCBlockHeader GenerateRandomVCBlockHeader();
FallbackBlockHeader GenerateRandomFallbackBlockHeader();
CoSignatures GenerateRandomCoSignatures();
Signature GetSignature(const std::vector<unsigned char>&, const PrivKey&,
                       const PubKey&);
Signature GenerateRandomSignature();

using DS_Comitte_t = std::deque<std::pair<PubKey, Peer>>;
DS_Comitte_t GenerateRandomDSCommittee(uint32_t);

Shard GenerateRandomShard(size_t);
DequeOfShard GenerateDequeueOfShard(size_t);
std::string GenerateRandomString(size_t);
std::vector<unsigned char> GenerateRandomCharVector(size_t);
}  // namespace TestUtils

#endif  // __TESTUTILS_H__
