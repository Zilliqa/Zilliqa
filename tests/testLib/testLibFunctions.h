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

#ifndef TESTS_TESTLIB_TESTLIBFUNCTIONS_H_
#define TESTS_TESTLIB_TESTLIBFUNCTIONS_H_

#include <limits>
#include <random>
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/DSBlockHeader.h"
#include "libData/BlockData/BlockHeader/MicroBlockHeader.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Peer.h"

static std::mt19937 rng;

template <typename T>
T randomIntInRng(T n, T m) {
  return std::uniform_int_distribution<T>{n, m}(rng);
}

uint8_t dist1to99();
uint16_t distUint16();
uint32_t distUint32();

PubKey generateRandomPubKey();
Peer generateRandomPeer();
DSBlockHeader generateRandomDSBlockHeader();
MicroBlockHeader generateRandomMicroBlockHeader();
TxBlockHeader generateRandomTxBlockHeader();
VCBlockHeader generateRandomVCBlockHeader();
FallbackBlockHeader generateRandomFallbackBlockHeader();
CoSignatures generateRandomCoSignatures();

#endif /* TESTS_TESTLIB_TESTFUNCTIONSLIB_H_ */
