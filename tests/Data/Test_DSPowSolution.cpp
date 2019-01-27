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

#include <boost/filesystem.hpp>

#include <string>
#include <vector>
#include "common/Constants.h"
#include "libData/MiningData/DSPowSolution.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libTestUtils/TestUtils.h"
#include "libCrypto/Schnorr.h"

#define BOOST_TEST_MODULE dspowsolutiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(dspowsolutiontest)

BOOST_AUTO_TEST_CASE(testDSPowSolutionClass) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  uint64_t blockNumberInput = TestUtils::DistUint64();
  uint8_t difficultyLevelInput = TestUtils::DistUint8();
  Peer submitterPeerInput = Peer();

  PairOfKey keypair = TestUtils::GenerateRandomKeyPair();
  bytes message = TestUtils::GenerateRandomCharVector(TestUtils::Dist1to99());
  Signature signatureInput = TestUtils::GetSignature(message, keypair);
  PubKey submitterKeyInput = keypair.second;

  uint64_t nonceInput = TestUtils::DistUint64();
  string resultingHashInput = TestUtils::GenerateRandomString(64);
  string mixHashInput = TestUtils::GenerateRandomString(64);
  uint32_t lookupIdInput = TestUtils::DistUint32();
  uint128_t gasPriceInput = TestUtils::DistUint128();



  DSPowSolution dsps(blockNumberInput,
                difficultyLevelInput,
                submitterPeerInput,submitterKeyInput,
                nonceInput,
                resultingHashInput,
                mixHashInput, lookupIdInput,
                gasPriceInput,
                signatureInput);

}

BOOST_AUTO_TEST_SUITE_END()
