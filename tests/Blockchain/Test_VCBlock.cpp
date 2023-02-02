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

#include "libBlockchain/VCBlock.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE vcblocktest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(vcblocktest)

BOOST_AUTO_TEST_CASE(VCBlock_DefaultConstruction) {
  VCBlock block;

  BOOST_CHECK_EQUAL(block.GetHeader(), VCBlockHeader{});
}

BOOST_AUTO_TEST_CASE(VCBlock_NonDefaultConstruction) {
  VCBlockHeader blockHeader{
      41,
      92,
      3,
      {4444, 5555},
      PubKey::GetPubKeyFromString(
          "872e4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa"),
      4,
      VectorOfNode{
          {PubKey::GetPubKeyFromString("bec5320d32a1a6c60a6258efa5e1b86c3dbf460"
                                       "af54cefe6e1ad4254ea8cb01cff"),
           {12345, 9937}}},
      1,  // version
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131"),
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61")};

  CoSignatures coSigs{3};
  VCBlock block{blockHeader, coSigs, 88};

  BOOST_CHECK_EQUAL(block.GetHeader(), blockHeader);
  BOOST_CHECK(block.GetB1() == coSigs.m_B1);
  BOOST_CHECK(block.GetB2() == coSigs.m_B2);
  BOOST_CHECK_EQUAL(block.GetCS1(), coSigs.m_CS1);
  BOOST_CHECK_EQUAL(block.GetCS2(), coSigs.m_CS2);
  BOOST_CHECK_EQUAL(block.GetTimestamp(), 88);
}

BOOST_AUTO_TEST_CASE(VCBlock_CompareEqual) {
  VCBlockHeader blockHeader1{
      5,
      6,
      7,
      {8888, 9999},
      PubKey::GetPubKeyFromString(
          "bec5320d32a1a6c60a6258efa5e1b86c3dbf460af54cefe6e1ad4254ea8cb01cff"),
      10,
      VectorOfNode{
          {PubKey::GetPubKeyFromString("872e4e50ce9990d8b041330c47c9ddd11bec6b5"
                                       "03ae9386a99da8584e9bb12c4aa"),

           {321, 1002}}},
      1,  // version
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61"),
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131")};

  CoSignatures coSigs1{8};
  VCBlock block1{blockHeader1, coSigs1, 412};

  auto blockHeader2 = blockHeader1;
  CoSignatures coSigs2 = coSigs1;
  VCBlock block2{blockHeader2, coSigs2, 412};

  auto block3 = block1;
  BOOST_CHECK_EQUAL(block1, block2);
  BOOST_CHECK_EQUAL(block1, block3);
  BOOST_CHECK_EQUAL(block2, block3);
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10,  141, 1,   10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149,
       13,  160, 204, 182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162,
       12,  188, 162, 124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,
       32,  145, 35,  220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198,
       141, 60,  162, 255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221,
       50,  184, 131, 225, 49,  16,  5,   24,  6,   32,  7,   42,  22,  10,
       20,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   3,   120, 0,   0,   3,   231, 50,  35,  10,  33,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   56,  11,  18,  192, 1,   10,  32,  92,  115, 78,  62,  3,
       21,  160, 87,  253, 43,  121, 25,  103, 238, 220, 79,  87,  99,  30,
       231, 195, 175, 173, 105, 3,   159, 201, 253, 79,  189, 186, 62,  18,
       146, 1,   10,  66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       18,  3,   0,   0,   0,   26,  66,  10,  64,  0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   34,  3,   0,   0,   0,   24,  128, 184, 226, 144, 231,
       190, 252, 2},
      {10,  204, 1,   10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149,
       13,  160, 204, 182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162,
       12,  188, 162, 124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,
       32,  145, 35,  220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198,
       141, 60,  162, 255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221,
       50,  184, 131, 225, 49,  16,  10,  24,  12,  32,  14,  42,  22,  10,
       20,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   6,   240, 0,   0,   7,   206, 50,  35,  10,  33,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   56,  12,  66,  61,  10,  35,  10,  33,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   18,  22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   58,  153, 0,   0,   92,  240, 18,  198, 1,
       10,  32,  192, 112, 169, 222, 248, 253, 181, 33,  84,  79,  22,  130,
       163, 52,  62,  216, 9,   174, 94,  203, 42,  7,   42,  163, 62,  72,
       192, 141, 183, 72,  50,  41,  18,  152, 1,   10,  66,  10,  64,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   18,  6,   0,   0,   0,   0,   0,
       0,   26,  66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   34,
       6,   0,   0,   0,   0,   0,   0,   24,  169, 188, 226, 144, 231, 190,
       252, 2},
      {10,  139, 2,   10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149,
       13,  160, 204, 182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162,
       12,  188, 162, 124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,
       32,  145, 35,  220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198,
       141, 60,  162, 255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221,
       50,  184, 131, 225, 49,  16,  15,  24,  18,  32,  21,  42,  22,  10,
       20,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   10,  104, 0,   0,   11,  181, 50,  35,  10,  33,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   56,  13,  66,  61,  10,  35,  10,  33,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   18,  22,  10,  20,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   58,  153, 0,   0,   92,  240, 66,  61,  10,
       35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   18,  22,  10,  20,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   58,  154,
       0,   0,   92,  241, 18,  204, 1,   10,  32,  175, 74,  189, 130, 126,
       138, 10,  140, 223, 168, 67,  145, 22,  211, 84,  207, 6,   6,   9,
       106, 171, 35,  220, 156, 131, 208, 129, 178, 137, 173, 138, 56,  18,
       158, 1,   10,  66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       18,  9,   0,   0,   0,   0,   0,   0,   0,   0,   0,   26,  66,  10,
       64,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   34,  9,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   24,  152, 191, 226, 144, 231, 190, 252,
       2}};

  uint64_t timestamps[3] = {1673415662017536, 1673415662018089,
                            1673415662018456};
  VectorOfNode faultyLeaders;
  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

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

    VCBlock block{blockHeader, CoSignatures(i * 3), timestamps[i - 1]};
    BOOST_CHECK(block.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

    VCBlock deserializedBlock;
    deserializedBlock.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlock.Deserialize(dst, 0));

    BOOST_CHECK(block == deserializedBlock);
    faultyLeaders.emplace_back(PubKey::GetPubKeyFromString(
                                   std::string(66, static_cast<char>('6' + i))),
                               Peer(i + 15000, i + 23791));
  }
}

BOOST_AUTO_TEST_SUITE_END()
