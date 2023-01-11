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

#include "libBlockchain/MicroBlock.h"

#define BOOST_TEST_MODULE microblocktest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(microblocktest)

BOOST_AUTO_TEST_CASE(MicroBlock_DefaultConstruction) {
  MicroBlock block;

  BOOST_CHECK_EQUAL(block.GetHeader(), MicroBlockHeader{});
}

BOOST_AUTO_TEST_CASE(MicroBlock_NonDefaultConstruction) {
  std::vector<TxnHash> tranHashes = {
      TxnHash{
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
      TxnHash{
          "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
      TxnHash{
          "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"}};

  MicroBlockHeader blockHeader{
      1,
      45,
      32,
      8,
      9122,
      MicroBlockHashSet{
          TxnHash{"4d99a6aad137aaad46f92e787f5d506e3249cf83cbbb9df23d38f049b986"
                  "3205"},
          StateHash{"a32fab43af733e2735a82b196f5530eb67f193a2a6140a5adecdd5eb3e"
                    "9f454a"},
          TxnHash{"99cba620b47e8bdad1a5aeb95d7b402c085074ee7c1724bd2936de10930b"
                  "636c"},
      },
      static_cast<uint32_t>(tranHashes.size()),
      PubKey::GetPubKeyFromString(
          "a0b54dfb242dbb7aabb5ab954e60125f4cfa12bc9aba5150f7c3012554d8de238a"),
      172,
      1,  // version
      BlockHash(
          "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52"),
      BlockHash(
          "e21a8a7b4f014090eaffd3e64dac41dcea4f5f7bbe67e0ac4deeb9f975130b87")};

  CoSignatures coSigs{5};
  MicroBlock block{blockHeader, tranHashes, coSigs, 13579};

  BOOST_CHECK_EQUAL(block.GetHeader(), blockHeader);
  BOOST_CHECK(block.GetB1() == coSigs.m_B1);
  BOOST_CHECK(block.GetB2() == coSigs.m_B2);
  BOOST_CHECK_EQUAL(block.GetCS1(), coSigs.m_CS1);
  BOOST_CHECK_EQUAL(block.GetCS2(), coSigs.m_CS2);
  BOOST_CHECK_EQUAL(block.GetTimestamp(), 13579);
  BOOST_TEST(block.GetTranHashes() == tranHashes);
}

BOOST_AUTO_TEST_CASE(MicroBlock_CompareEqual) {
  std::vector<TxnHash> tranHashes = {
      TxnHash{
          "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
      TxnHash{
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}};

  MicroBlockHeader blockHeader1{
      1,
      45,
      32,
      8,
      9122,
      {
          TxnHash{"4d99a6aad137aaad46f92e787f5d506e3249cf83cbbb9df23d38f049b986"
                  "3205"},
          StateHash{"a32fab43af733e2735a82b196f5530eb67f193a2a6140a5adecdd5eb3e"
                    "9f454a"},
          TxnHash{"99cba620b47e8bdad1a5aeb95d7b402c085074ee7c1724bd2936de10930b"
                  "636c"},
      },
      static_cast<uint32_t>(tranHashes.size()),
      PubKey::GetPubKeyFromString(
          "9ab33a3868993176b613738816247a7f4d357cae555996519cf5b543e9b3554b89"),
      172,
      1,  // version
      BlockHash(
          "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52"),
      BlockHash(
          "e21a8a7b4f014090eaffd3e64dac41dcea4f5f7bbe67e0ac4deeb9f975130b87")};

  CoSignatures coSigs1{5};
  MicroBlock block1{blockHeader1, tranHashes, coSigs1, 24633};

  auto blockHeader2 = blockHeader1;
  CoSignatures coSigs2 = coSigs1;
  MicroBlock block2{blockHeader2, tranHashes, coSigs2, 24633};

  auto block3 = block1;
  BOOST_CHECK_EQUAL(block1, block2);
  BOOST_CHECK_EQUAL(block1, block3);
  BOOST_CHECK_EQUAL(block2, block3);
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10,  245, 1,   10,  70,  8,   1,   18,  32,  139, 125, 241, 67,  217,
       28,  113, 110, 207, 165, 252, 23,  48,  2,   47,  107, 66,  27,  5,
       206, 222, 232, 253, 82,  177, 252, 101, 169, 96,  48,  173, 82,  26,
       32,  226, 26,  138, 123, 79,  1,   64,  144, 234, 255, 211, 230, 77,
       172, 65,  220, 234, 79,  95,  123, 190, 103, 224, 172, 77,  238, 185,
       249, 117, 19,  11,  135, 16,  1,   24,  9,   32,  8,   42,  18,  10,
       16,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   7,   56,  141, 106, 66,  32,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   72,  0,
       82,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   88,  130, 45,  98,  32,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   106, 32,  0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   26,  192, 1,   10,
       32,  246, 215, 20,  219, 238, 149, 239, 225, 38,  61,  132, 224, 90,
       135, 188, 96,  84,  131, 125, 248, 83,  251, 183, 38,  254, 230, 178,
       58,  2,   134, 116, 194, 18,  146, 1,   10,  66,  10,  64,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   18,  3,   0,   0,   0,   26,  66,  10,
       64,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   34,  3,   0,   0,   0,
       24,  143, 212, 254, 190, 214, 190, 252, 2},
      {10,  245, 1,   10,  70,  8,   1,   18,  32,  139, 125, 241, 67,  217,
       28,  113, 110, 207, 165, 252, 23,  48,  2,   47,  107, 66,  27,  5,
       206, 222, 232, 253, 82,  177, 252, 101, 169, 96,  48,  173, 82,  26,
       32,  226, 26,  138, 123, 79,  1,   64,  144, 234, 255, 211, 230, 77,
       172, 65,  220, 234, 79,  95,  123, 190, 103, 224, 172, 77,  238, 185,
       249, 117, 19,  11,  135, 16,  2,   24,  18,  32,  16,  42,  18,  10,
       16,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   14,  56,  142, 106, 66,  32,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   72,  1,
       82,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   88,  131, 45,  98,  32,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   106, 32,  0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  32,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   26,  198, 1,   10,  32,  53,  117, 66,  134, 229, 90,  23,
       174, 84,  248, 106, 39,  16,  29,  245, 71,  185, 144, 24,  97,  66,
       178, 124, 247, 1,   156, 11,  209, 135, 79,  31,  93,  18,  152, 1,
       10,  66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  6,
       0,   0,   0,   0,   0,   0,   26,  66,  10,  64,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   34,  6,   0,   0,   0,   0,   0,   0,   24,  152,
       216, 254, 190, 214, 190, 252, 2},
      {10,  245, 1,   10,  70,  8,   1,   18,  32,  139, 125, 241, 67,  217,
       28,  113, 110, 207, 165, 252, 23,  48,  2,   47,  107, 66,  27,  5,
       206, 222, 232, 253, 82,  177, 252, 101, 169, 96,  48,  173, 82,  26,
       32,  226, 26,  138, 123, 79,  1,   64,  144, 234, 255, 211, 230, 77,
       172, 65,  220, 234, 79,  95,  123, 190, 103, 224, 172, 77,  238, 185,
       249, 117, 19,  11,  135, 16,  3,   24,  27,  32,  24,  42,  18,  10,
       16,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   21,  56,  143, 106, 66,  32,  0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   72,  2,
       82,  35,  10,  33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   88,  132, 45,  98,  32,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   106, 32,  0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   18,  32,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   18,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   26,  204, 1,   10,  32,  0,
       186, 42,  213, 121, 92,  100, 97,  134, 153, 130, 117, 151, 130, 132,
       64,  237, 95,  94,  184, 115, 132, 119, 69,  254, 140, 130, 54,  195,
       10,  248, 208, 18,  158, 1,   10,  66,  10,  64,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   18,  9,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   26,  66,  10,  64,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   34,
       9,   0,   0,   0,   0,   0,   0,   0,   0,   0,   24,  250, 218, 254,
       190, 214, 190, 252, 2}};

  std::vector<TxnHash> tranHashes;
  uint64_t timestamps[3] = {1673411195546127, 1673411195546648,
                            1673411195547002};
  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

    MicroBlockHeader blockHeader{
        static_cast<uint32_t>(i),
        static_cast<uint64_t>(i * 9),
        static_cast<uint64_t>(i * 8),
        static_cast<unsigned char>(i * 7),
        static_cast<uint64_t>(i + 13580),
        {TxnHash{std::string(32, static_cast<char>('1' + i))},
         StateHash{std::string(32, static_cast<char>('a' + i))},
         TxnHash{std::string(32, static_cast<char>('5' + i))}},
        static_cast<uint32_t>(tranHashes.size()),
        PubKey::GetPubKeyFromString(
            std::string(66, static_cast<char>('1' + i))),
        static_cast<uint64_t>(i + 5761),
        1,  // version
        BlockHash(
            "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52"),
        BlockHash("e21a8a7b4f014090eaffd3e64dac41dcea4f5f7bbe67e0ac4deeb9f97513"
                  "0b87")};

    MicroBlock block{blockHeader, tranHashes, CoSignatures(i * 3),
                     timestamps[i - 1]};
    BOOST_CHECK(block.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

    MicroBlock deserializedBlock;
    deserializedBlock.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlock.Deserialize(dst, 0));

    BOOST_CHECK(block == deserializedBlock);
    tranHashes.emplace_back(
        TxnHash{std::string(32, static_cast<char>('a' + i))});
  }
}

BOOST_AUTO_TEST_SUITE_END()
