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

#include <string>
#include <vector>
#include "common/Constants.h"
#include "libData/MiningData/DSPowSolution.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE dspowsolutiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(dspowsolutiontest)

BOOST_AUTO_TEST_CASE(testDSPowSolutionClass) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  // Satisfy linker
  Account acc1(0, 0);

  uint64_t blockNumberInput = TestUtils::DistUint64();
  uint8_t difficultyLevelInput = TestUtils::DistUint8();
  Peer submitterPeerInput = Peer();
  PairOfKey keypair = TestUtils::GenerateRandomKeyPair();
  bytes message = TestUtils::GenerateRandomCharVector(TestUtils::Dist1to99());
  PairOfKey sender = Schnorr::GenKeyPair();
  Signature signatureInput = TestUtils::GetSignature(message, keypair);
  bytes message2 = TestUtils::GenerateRandomCharVector(TestUtils::Dist1to99());
  Signature signature2 = TestUtils::GetSignature(message, keypair);
  PubKey submitterKeyInput = keypair.second;
  uint64_t nonceInput = TestUtils::DistUint64();
  string resultingHashInput = TestUtils::GenerateRandomString(64);
  string mixHashInput = TestUtils::GenerateRandomString(64);
  uint32_t lookupIdInput = TestUtils::DistUint32();
  uint128_t gasPriceInput = TestUtils::DistUint128();
  DSPowSolution dsps;

  dsps =
      DSPowSolution(blockNumberInput, difficultyLevelInput, submitterPeerInput,
                    submitterKeyInput, nonceInput, resultingHashInput,
                    mixHashInput, lookupIdInput, gasPriceInput, signatureInput);

  DSPowSolution dsps2 = dsps;
  BOOST_REQUIRE(dsps2 == dsps);

  BOOST_REQUIRE(blockNumberInput == dsps.GetBlockNumber());
  BOOST_REQUIRE(dsps.GetDifficultyLevel() == difficultyLevelInput);
  BOOST_REQUIRE(dsps.GetSubmitterPeer() == submitterPeerInput);
  BOOST_REQUIRE(dsps.GetSubmitterKey() == submitterKeyInput);
  BOOST_REQUIRE(dsps.GetNonce() == nonceInput);
  BOOST_REQUIRE(dsps.GetResultingHash() == resultingHashInput);
  BOOST_REQUIRE(dsps.GetMixHash() == mixHashInput);
  BOOST_REQUIRE(dsps.GetLookupId() == lookupIdInput);
  BOOST_REQUIRE(dsps.GetGasPrice() == gasPriceInput);
  BOOST_REQUIRE(dsps.GetSignature() == signatureInput);
  dsps.SetSignature(signature2);
  BOOST_REQUIRE(dsps.GetSignature() == signature2);

  // Cover copy constructor
  DSPowSolution dsps3;
  dsps3 = dsps2;
  BOOST_REQUIRE(dsps2 == dsps3);
}

BOOST_AUTO_TEST_SUITE_END()
