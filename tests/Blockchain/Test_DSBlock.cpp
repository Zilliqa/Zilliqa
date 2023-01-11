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

#include "libBlockchain/DSBlock.h"

#define BOOST_TEST_MODULE dsblocktest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(dsblocktest)

BOOST_AUTO_TEST_CASE(DSBlock_DefaultConstruction) {
  DSBlock block;

  BOOST_CHECK_EQUAL(block.GetHeader(), DSBlockHeader{});
}

BOOST_AUTO_TEST_CASE(DSBlock_NonDefaultConstruction) {
  DSBlockHeader blockHeader{
      41,
      92,
      PubKey::GetPubKeyFromString(
          "872e4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa"),
      33,
      89,
      111,
      SWInfo{},
      {// PoW winners
       {PubKey::GetPubKeyFromString("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                                    "bbbbbbbbbbbbbbbbbbbbbbbb"),
        Peer{8888, 1111}}},
      {// Removed keys
       PubKey::GetPubKeyFromString("ccccccccccccccccccccccccccccccccccccccccccc"
                                   "ccccccccccccccccccccccc")},
      {},
      {},
      1,  // version
      BlockHash(
          "c22b1ab817891c54a3e3c2bb1e1e09a9a616cb2a763f8027cd8646ec1ee038e6"),
      BlockHash(
          "677dc8f0cbe535e8ee53ea9bb8a0f2517857bc827fe8aed9aba734d8d5d2f282")};

  CoSignatures coSigs{5};
  DSBlock block{blockHeader, coSigs, 13579};

  BOOST_CHECK_EQUAL(block.GetHeader(), blockHeader);
  BOOST_CHECK(block.GetB1() == coSigs.m_B1);
  BOOST_CHECK(block.GetB2() == coSigs.m_B2);
  BOOST_CHECK_EQUAL(block.GetCS1(), coSigs.m_CS1);
  BOOST_CHECK_EQUAL(block.GetCS2(), coSigs.m_CS2);
  BOOST_CHECK_EQUAL(block.GetTimestamp(), 13579);
}

BOOST_AUTO_TEST_CASE(DSBlock_CompareEqual) {
  DSBlockHeader blockHeader1{
      9,
      2,
      PubKey::GetPubKeyFromString(
          "9fff4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa"),
      9,
      10,
      555,
      SWInfo{},
      {// PoW winners
       {PubKey::GetPubKeyFromString("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                                    "bbbbbbbbbbbbbbbbbbbbbbbb"),
        Peer{13579, 35000}}},
      {// Removed keys
       PubKey::GetPubKeyFromString("ccccccccccccccccccccccccccccccccccccccccccc"
                                   "ccccccccccccccccccccccc")},
      {},
      {},
      1,  // version
      BlockHash(
          "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
      BlockHash(
          "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")};

  CoSignatures coSigs1{5};
  DSBlock block1{blockHeader1, coSigs1, 24633};

  auto blockHeader2 = blockHeader1;
  CoSignatures coSigs2 = coSigs1;
  DSBlock block2{blockHeader1, coSigs2, 24633};

  auto block3 = block1;
  BOOST_CHECK_EQUAL(block1, block2);
  BOOST_CHECK_EQUAL(block1, block3);
  BOOST_CHECK_EQUAL(block2, block3);
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10,  201, 3,   10,  70,  8,   1,   18,  32,  187, 187, 187, 187, 187,
       187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
       187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 26,
       32,  103, 125, 200, 240, 203, 229, 53,  232, 238, 83,  234, 155, 184,
       160, 242, 81,  120, 87,  188, 130, 127, 232, 174, 217, 171, 167, 52,
       216, 213, 210, 242, 130, 16,  5,   24,  3,   42,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   48,  21,  56,  25,  66,  18,  10,  16,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   76,
       74,  50,  10,  48,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   82,  61,  10,  35,
       10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   18,  22,  10,  20,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   112, 0,
       0,   8,   227, 90,  165, 1,   10,  32,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  128,
       1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   98,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  192,
       1,   10,  32,  190, 207, 48,  11,  87,  82,  148, 209, 59,  141, 187,
       28,  255, 34,  249, 5,   88,  12,  140, 15,  140, 141, 74,  178, 197,
       53,  92,  130, 144, 95,  165, 120, 18,  146, 1,   10,  66,  10,  64,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   18,  3,   0,   0,   0,   26,
       66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   34,  3,   0,
       0,   0,   24,  172, 245, 139, 201, 173, 188, 252, 2},
      {10,  201, 3,   10,  70,  8,   1,   18,  32,  204, 204, 204, 204, 204,
       204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
       204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 26,
       32,  103, 125, 200, 240, 203, 229, 53,  232, 238, 83,  234, 155, 184,
       160, 242, 81,  120, 87,  188, 130, 127, 232, 174, 217, 171, 167, 52,
       216, 213, 210, 242, 130, 16,  10,  24,  6,   42,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   48,  22,  56,  25,  66,  18,  10,  16,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   76,
       74,  50,  10,  48,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   82,  61,  10,  35,
       10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   18,  22,  10,  20,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   113, 0,
       0,   8,   227, 90,  165, 1,   10,  32,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  128,
       1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   98,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  198,
       1,   10,  32,  190, 119, 145, 98,  42,  0,   79,  79,  100, 212, 214,
       82,  132, 151, 7,   178, 189, 26,  150, 110, 85,  149, 150, 82,  238,
       21,  114, 144, 102, 153, 89,  181, 18,  152, 1,   10,  66,  10,  64,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   18,  6,   0,   0,   0,   0,
       0,   0,   26,  66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       34,  6,   0,   0,   0,   0,   0,   0,   24,  153, 254, 139, 201, 173,
       188, 252, 2},
      {10,  201, 3,   10,  70,  8,   1,   18,  32,  221, 221, 221, 221, 221,
       221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
       221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 26,
       32,  103, 125, 200, 240, 203, 229, 53,  232, 238, 83,  234, 155, 184,
       160, 242, 81,  120, 87,  188, 130, 127, 232, 174, 217, 171, 167, 52,
       216, 213, 210, 242, 130, 16,  15,  24,  9,   42,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   48,  23,  56,  25,  66,  18,  10,  16,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   76,
       74,  50,  10,  48,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   82,  61,  10,  35,
       10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   18,  22,  10,  20,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   114, 0,
       0,   8,   227, 90,  165, 1,   10,  32,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  128,
       1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   98,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  204,
       1,   10,  32,  223, 157, 107, 117, 236, 27,  8,   18,  216, 216, 51,
       156, 147, 6,   29,  125, 202, 200, 27,  77,  24,  183, 226, 245, 214,
       200, 69,  107, 238, 173, 144, 128, 18,  158, 1,   10,  66,  10,  64,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   18,  9,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   26,  66,  10,  64,  0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   34,  9,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       24,  150, 134, 140, 201, 173, 188, 252, 2}};

  uint64_t timestamps[3] = {1673331491404460, 1673331491405593,
                            1673331491406614};
  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

    DSBlockHeader blockHeader{
        static_cast<uint8_t>(i * 5),
        static_cast<uint8_t>(i * 3),
        PubKey::GetPubKeyFromString(
            std::string(66, static_cast<char>('1' + i))),
        static_cast<uint64_t>(i + 20),
        25,
        76,
        SWInfo{},
        {// PoW winners
         {PubKey::GetPubKeyFromString(
              std::string(66, static_cast<char>('3' + i))),
          Peer{111 + i, 2275}}},
        {
            // Removed keys
            PubKey::GetPubKeyFromString(
                std::string(66, static_cast<char>('2' + i))),
        },
        {},
        {},
        1,  // version
        BlockHash(std::string(64, static_cast<char>('a' + i))),
        BlockHash("677dc8f0cbe535e8ee53ea9bb8a0f2517857bc827fe8aed9aba734d8d5d2"
                  "f282")};

    DSBlock block{blockHeader, CoSignatures(i * 3), timestamps[i - 1]};
    BOOST_CHECK(block.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

    DSBlock deserializedBlock;
    deserializedBlock.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlock.Deserialize(dst, 0));

    BOOST_CHECK(block == deserializedBlock);
  }
}

BOOST_AUTO_TEST_SUITE_END()
