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

#include "libData/BlockData/BlockHeader/VCBlockHeader.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE vcblockheadertest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(vcblockheadertest)

BOOST_AUTO_TEST_CASE(VCBlockHeader_DefaultConstruction) {
  VCBlockHeader blockHeader;

  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeDSEpochNo(),
                    static_cast<uint64_t>(-1));
  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeEpochNo(),
                    static_cast<uint64_t>(-1));
  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeState(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetCandidateLeaderNetworkInfo(), Peer{});
  BOOST_CHECK_EQUAL(blockHeader.GetCandidateLeaderPubKey(), PubKey{});
  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeCounter(), 0);
  BOOST_CHECK(blockHeader.GetFaultyLeaders().empty());
}

BOOST_AUTO_TEST_CASE(VCBlockHeader_NonDefaultConstruction) {
  auto candidateLeaderPubKey = PubKey::GetPubKeyFromString(
      "872e4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa");
  auto faultyLeaderPubKey = PubKey::GetPubKeyFromString(
      "bec5320d32a1a6c60a6258efa5e1b86c3dbf460af54cefe6e1ad4254ea8cb01cff");
  VectorOfNode faultyLeaders{{faultyLeaderPubKey, {12345, 9937}}};
  VCBlockHeader blockHeader{
      41,
      92,
      3,
      {4444, 5555},
      candidateLeaderPubKey,
      4,
      faultyLeaders,
      1,  // version
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131"),
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61")};

  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeDSEpochNo(), 41);
  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeEpochNo(), 92);
  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeState(), 3);
  BOOST_CHECK_EQUAL(blockHeader.GetCandidateLeaderNetworkInfo(),
                    Peer(4444, 5555));
  BOOST_CHECK_EQUAL(blockHeader.GetCandidateLeaderPubKey(),
                    candidateLeaderPubKey);
  BOOST_CHECK_EQUAL(blockHeader.GetViewChangeCounter(), 4);
  BOOST_TEST(blockHeader.GetFaultyLeaders() == faultyLeaders);
}

BOOST_AUTO_TEST_CASE(VCBlockHeader_CompareEqual) {
  auto candidateLeaderPubKey = PubKey::GetPubKeyFromString(
      "bec5320d32a1a6c60a6258efa5e1b86c3dbf460af54cefe6e1ad4254ea8cb01cff");
  auto faultyLeaderPubKey = PubKey::GetPubKeyFromString(
      "872e4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa");
  VectorOfNode faultyLeaders{{faultyLeaderPubKey, {321, 1002}}};
  VCBlockHeader blockHeader1{
      5,
      6,
      7,
      {8888, 9999},
      candidateLeaderPubKey,
      10,
      faultyLeaders,
      1,  // version
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61"),
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131")};

  auto blockHeader2 = blockHeader1;

  BOOST_CHECK_EQUAL(blockHeader1, blockHeader2);
  BOOST_CHECK_EQUAL(blockHeader2, blockHeader1);

  BOOST_CHECK_EQUAL(VCBlockHeader{}, VCBlockHeader{});
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10, 70,  8,   1,   18,  32,  113, 122, 197, 6,   149, 13,  160, 204, 182,
       64, 76,  221, 94,  117, 145, 247, 32,  24,  162, 12,  188, 162, 124, 138,
       66, 62,  156, 158, 86,  38,  172, 97,  26,  32,  145, 35,  220, 187, 11,
       66, 101, 43,  14,  16,  89,  86,  198, 141, 60,  162, 255, 52,  88,  79,
       50, 79,  164, 26,  41,  174, 221, 50,  184, 131, 225, 49,  16,  5,   24,
       6,  32,  7,   42,  22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   3,   120, 0,   0,   3,   231, 50,  35,  10,
       33, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   56,  11,  66,  61,  10,  35,  10,  33,  0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       18, 22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   58,  153, 0,   0,   92,  240},
      {10, 70,  8,   1,   18,  32,  113, 122, 197, 6,   149, 13,  160, 204, 182,
       64, 76,  221, 94,  117, 145, 247, 32,  24,  162, 12,  188, 162, 124, 138,
       66, 62,  156, 158, 86,  38,  172, 97,  26,  32,  145, 35,  220, 187, 11,
       66, 101, 43,  14,  16,  89,  86,  198, 141, 60,  162, 255, 52,  88,  79,
       50, 79,  164, 26,  41,  174, 221, 50,  184, 131, 225, 49,  16,  10,  24,
       12, 32,  14,  42,  22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   6,   240, 0,   0,   7,   206, 50,  35,  10,
       33, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   56,  12,  66,  61,  10,  35,  10,  33,  0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       18, 22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   58,  153, 0,   0,   92,  240, 66,  61,  10,  35,  10,  33,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   18,  22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,
       0,  0,   0,   0,   0,   0,   58,  154, 0,   0,   92,  241},
      {10,  70,  8,  1,   18,  32,  113, 122, 197, 6,   149, 13,  160, 204,
       182, 64,  76, 221, 94,  117, 145, 247, 32,  24,  162, 12,  188, 162,
       124, 138, 66, 62,  156, 158, 86,  38,  172, 97,  26,  32,  145, 35,
       220, 187, 11, 66,  101, 43,  14,  16,  89,  86,  198, 141, 60,  162,
       255, 52,  88, 79,  50,  79,  164, 26,  41,  174, 221, 50,  184, 131,
       225, 49,  16, 15,  24,  18,  32,  21,  42,  22,  10,  20,  0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  104,
       0,   0,   11, 181, 50,  35,  10,  33,  0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   56,
       13,  66,  61, 10,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  22,
       10,  20,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   58, 153, 0,   0,   92,  240, 66,  61,  10,  35,  10,  33,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   18,  22,  10,  20,  0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   58,  154, 0,   0,   92,
       241, 66,  61, 10,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  22,
       10,  20,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   58, 155, 0,   0,   92,  242}};

  VectorOfNode faultyLeaders;
  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

    faultyLeaders.emplace_back(PubKey::GetPubKeyFromString(
                                   std::string(66, static_cast<char>('6' + i))),
                               Peer(i + 15000, i + 23791));
    VCBlockHeader blockHeader{
        static_cast<uint64_t>(i * 5),
        static_cast<uint64_t>(i * 6),
        static_cast<unsigned char>(i * 7),
        {i * 888, static_cast<uint32_t>(i * 999)},
        PubKey::GetPubKeyFromString(
            std::string(66, static_cast<char>('1' + i))),
        static_cast<uint32_t>(i + 10),
        faultyLeaders,
        1,  // version
        BlockHash(
            "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61"),
        BlockHash("9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883"
                  "e131")};

    BOOST_CHECK(blockHeader.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

    VCBlockHeader deserializedBlockHeader;
    deserializedBlockHeader.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlockHeader.Deserialize(dst, 0));

    BOOST_CHECK(blockHeader == deserializedBlockHeader);
  }
}

BOOST_AUTO_TEST_SUITE_END()
