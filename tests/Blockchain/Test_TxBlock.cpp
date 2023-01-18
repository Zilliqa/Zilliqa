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

#include "libBlockchain/TxBlock.h"

#define BOOST_TEST_MODULE txblocktest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(txblocktest)

BOOST_AUTO_TEST_CASE(TxBlock_DefaultConstruction) {
  TxBlock block;

  BOOST_CHECK_EQUAL(block.GetHeader(), TxBlockHeader{});
}

BOOST_AUTO_TEST_CASE(TxBlock_NonDefaultConstruction) {
  std::vector<MicroBlockInfo> mbInfos{
      {BlockHash{
           "8888888888888888888888888888888888888888888888888888888888888888"},
       TxnHash{
           "9999999999999999999999999999999999999999999999999999999999999999"},
       1},
      {BlockHash{
           "7777777777777777777777777777777777777777777777777777777777777777"},
       TxnHash{
           "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"},
       2}};

  TxBlockHeader blockHeader{
      54,
      23,
      3,
      1235,
      {},
      9,
      PubKey::GetPubKeyFromString(
          "8b133a3868993176b613738816247a7f4d357cae555996519cf5b543e9b3554b89"),
      211,
      1,  // version
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131"),
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61")};

  CoSignatures coSigs{5};
  TxBlock block{blockHeader, mbInfos, coSigs, 5239};

  BOOST_CHECK_EQUAL(block.GetHeader(), blockHeader);
  BOOST_CHECK(block.GetB1() == coSigs.m_B1);
  BOOST_CHECK(block.GetB2() == coSigs.m_B2);
  BOOST_CHECK_EQUAL(block.GetCS1(), coSigs.m_CS1);
  BOOST_CHECK_EQUAL(block.GetCS2(), coSigs.m_CS2);
  BOOST_CHECK_EQUAL(block.GetTimestamp(), 5239);
  BOOST_TEST(block.GetMicroBlockInfos() == mbInfos);
}

BOOST_AUTO_TEST_CASE(TxBlock_CompareEqual) {
  std::vector<MicroBlockInfo> mbInfos{
      {BlockHash{
           "0000000000000000000000000000000000000000000000000000000000000000"},
       TxnHash{
           "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
       3},
      {BlockHash{
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
       TxnHash{
           "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"},
       9}};

  TxBlockHeader blockHeader1{
      5,
      2,
      0,
      235,
      {},
      8,
      PubKey::GetPubKeyFromString(
          "9ab33a3868993176b613738816247a7f4d357cae555996519cf5b543e9b3554b89"),
      11,
      1,  // version
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131"),
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61")};

  CoSignatures coSigs1{5};
  TxBlock block1{blockHeader1, mbInfos, coSigs1, 1115};

  auto blockHeader2 = blockHeader1;
  CoSignatures coSigs2 = coSigs1;
  TxBlock block2{blockHeader2, mbInfos, coSigs2, 1115};

  auto block3 = block1;
  BOOST_CHECK_EQUAL(block1, block2);
  BOOST_CHECK_EQUAL(block1, block3);
  BOOST_CHECK_EQUAL(block2, block3);
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10,  244, 1,   10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149,
       13,  160, 204, 182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162,
       12,  188, 162, 124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,
       32,  145, 35,  220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198,
       141, 60,  162, 255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221,
       50,  184, 131, 225, 49,  16,  9,   24,  8,   34,  18,  10,  16,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       7,   48,  10,  58,  102, 10,  32,  207, 116, 107, 11,  134, 184, 80,
       247, 113, 178, 117, 58,  163, 187, 245, 108, 214, 2,   222, 1,   31,
       29,  74,  79,  36,  173, 101, 63,  183, 210, 73,  150, 18,  32,  255,
       124, 156, 249, 165, 117, 10,  25,  41,  116, 214, 226, 9,   148, 77,
       78,  168, 107, 163, 90,  106, 41,  173, 148, 112, 93,  134, 182, 79,
       229, 204, 203, 26,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   64,  136, 1,   74,  35,
       10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   80,  20,  26,  192, 1,   10,  32,
       128, 65,  249, 89,  77,  152, 224, 208, 138, 40,  97,  129, 172, 19,
       238, 18,  29,  180, 208, 3,   192, 194, 255, 255, 236, 157, 189, 7,
       57,  213, 174, 247, 18,  146, 1,   10,  66,  10,  64,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   18,  3,   0,   0,   0,   26,  66,  10,  64,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   34,  3,   0,   0,   0,   24,
       135, 156, 183, 203, 224, 190, 252, 2},
      {10,  244, 1,   10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149,
       13,  160, 204, 182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162,
       12,  188, 162, 124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,
       32,  145, 35,  220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198,
       141, 60,  162, 255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221,
       50,  184, 131, 225, 49,  16,  18,  24,  16,  34,  18,  10,  16,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       14,  48,  20,  58,  102, 10,  32,  207, 116, 107, 11,  134, 184, 80,
       247, 113, 178, 117, 58,  163, 187, 245, 108, 214, 2,   222, 1,   31,
       29,  74,  79,  36,  173, 101, 63,  183, 210, 73,  150, 18,  32,  255,
       124, 156, 249, 165, 117, 10,  25,  41,  116, 214, 226, 9,   148, 77,
       78,  168, 107, 163, 90,  106, 41,  173, 148, 112, 93,  134, 182, 79,
       229, 204, 203, 26,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   64,  137, 1,   74,  35,
       10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   80,  21,  18,  70,  10,  32,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   18,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   24,  1,   26,  198, 1,
       10,  32,  45,  72,  25,  192, 170, 243, 12,  112, 158, 44,  102, 116,
       101, 80,  79,  82,  189, 98,  81,  102, 22,  59,  187, 231, 36,  43,
       189, 53,  84,  159, 81,  163, 18,  152, 1,   10,  66,  10,  64,  0,
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
       6,   0,   0,   0,   0,   0,   0,   24,  216, 160, 183, 203, 224, 190,
       252, 2},
      {10,  244, 1,   10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149,
       13,  160, 204, 182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162,
       12,  188, 162, 124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,
       32,  145, 35,  220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198,
       141, 60,  162, 255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221,
       50,  184, 131, 225, 49,  16,  27,  24,  24,  34,  18,  10,  16,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       21,  48,  30,  58,  102, 10,  32,  207, 116, 107, 11,  134, 184, 80,
       247, 113, 178, 117, 58,  163, 187, 245, 108, 214, 2,   222, 1,   31,
       29,  74,  79,  36,  173, 101, 63,  183, 210, 73,  150, 18,  32,  255,
       124, 156, 249, 165, 117, 10,  25,  41,  116, 214, 226, 9,   148, 77,
       78,  168, 107, 163, 90,  106, 41,  173, 148, 112, 93,  134, 182, 79,
       229, 204, 203, 26,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   64,  138, 1,   74,  35,
       10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   80,  22,  18,  70,  10,  32,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   18,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   24,  1,   18,  70,  10,
       32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   18,  32,  0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   24,  2,   26,
       204, 1,   10,  32,  142, 247, 105, 137, 104, 30,  190, 182, 222, 166,
       72,  127, 132, 70,  246, 221, 193, 126, 125, 234, 66,  197, 54,  114,
       19,  235, 250, 119, 196, 170, 246, 163, 18,  158, 1,   10,  66,  10,
       64,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  9,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   26,  66,  10,  64,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   34,  9,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   24,  222, 163, 183, 203, 224, 190, 252, 2}};

  std::vector<MicroBlockInfo> mbInfos;
  uint64_t timestamps[3] = {1673413905993223, 1673413905993816,
                            1673413905994206};
  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

    TxBlockHeader blockHeader{
        static_cast<uint64_t>(i * 9),
        static_cast<uint64_t>(i * 8),
        static_cast<unsigned char>(i * 7),
        static_cast<uint64_t>(i * 10),
        {BlockHash("cf746b0b86b850f771b2753aa3bbf56cd602de011f1d4a4f24ad653fb7d"
                   "24996"),
         BlockHash("ff7c9cf9a5750a192974d6e209944d4ea86ba35a6a29ad94705d86b64fe"
                   "5cccb"),
         {}},
        static_cast<uint32_t>(i + 135),
        PubKey::GetPubKeyFromString(
            std::string(66, static_cast<char>('1' + i))),
        static_cast<uint32_t>(i + 19),
        1,  // version
        BlockHash(
            "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61"),
        BlockHash("9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883"
                  "e131")};

    TxBlock block{blockHeader, mbInfos, CoSignatures(i * 3), timestamps[i - 1]};
    BOOST_CHECK(block.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

    TxBlock deserializedBlock;
    deserializedBlock.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlock.Deserialize(dst, 0));

    BOOST_CHECK(block == deserializedBlock);
    mbInfos.emplace_back(
        MicroBlockInfo{BlockHash{std::string(32, static_cast<char>('0' + i))},
                       TxnHash{std::string(32, static_cast<char>('3' + i))},
                       static_cast<uint32_t>(i)});
  }
}

BOOST_AUTO_TEST_SUITE_END()
