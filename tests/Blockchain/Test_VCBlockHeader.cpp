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

#include <fstream>

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
      {10,  70,  8,   1,   18,  32,  187, 187, 187, 187, 187, 187, 187, 187,
       187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
       187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 26,  32,  103, 125,
       200, 240, 203, 229, 53,  232, 238, 83,  234, 155, 184, 160, 242, 81,
       120, 87,  188, 130, 127, 232, 174, 217, 171, 167, 52,  216, 213, 210,
       242, 130, 16,  5,   24,  3,   42,  35,  10,  33,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   48,  21,  56,  25,  66,  18,  10,  16,  0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   76,  74,  50,  10,
       48,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   82,  61,  10,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   18,  22,  10,  20,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   112, 0,   0,   8,   227,
       90,  165, 1,   10,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  128, 1,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       98,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0},
      {10,  70,  8,   1,   18,  32,  204, 204, 204, 204, 204, 204, 204, 204,
       204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
       204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 26,  32,  103, 125,
       200, 240, 203, 229, 53,  232, 238, 83,  234, 155, 184, 160, 242, 81,
       120, 87,  188, 130, 127, 232, 174, 217, 171, 167, 52,  216, 213, 210,
       242, 130, 16,  10,  24,  6,   42,  35,  10,  33,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   48,  22,  56,  25,  66,  18,  10,  16,  0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   76,  74,  50,  10,
       48,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   82,  61,  10,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   18,  22,  10,  20,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   113, 0,   0,   8,   227,
       90,  165, 1,   10,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  128, 1,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       98,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0},
      {10,  70,  8,   1,   18,  32,  221, 221, 221, 221, 221, 221, 221, 221,
       221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
       221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 26,  32,  103, 125,
       200, 240, 203, 229, 53,  232, 238, 83,  234, 155, 184, 160, 242, 81,
       120, 87,  188, 130, 127, 232, 174, 217, 171, 167, 52,  216, 213, 210,
       242, 130, 16,  15,  24,  9,   42,  35,  10,  33,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   48,  23,  56,  25,  66,  18,  10,  16,  0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   76,  74,  50,  10,
       48,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   82,  61,  10,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   18,  22,  10,  20,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   111, 0,   0,   8,   227,
       90,  165, 1,   10,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  128, 1,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       98,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0}};

  VectorOfNode faultyLeaders;
  for (int i = 1; i < 3; ++i) {
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
#if 0
    BOOST_TEST(dst == serialized[i - 1]);

    std::ofstream file{"dst-" + std::to_string(i)};
    file << '{';
    for (auto v : dst) file << static_cast<unsigned int>(v) << ", ";
    file << '}';
#endif

    VCBlockHeader deserializedBlockHeader;
    deserializedBlockHeader.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlockHeader.Deserialize(dst, 0));

    BOOST_CHECK(blockHeader == deserializedBlockHeader);
  }
}

BOOST_AUTO_TEST_SUITE_END()
