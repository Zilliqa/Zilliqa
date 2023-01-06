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

#include "libData/BlockData/BlockHeader/MicroBlockHeader.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE mbblockheadertest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(mbblockheadertest)

BOOST_AUTO_TEST_CASE(MicroBlockHeader_DefaultConstruction) {
  MicroBlockHeader blockHeader;

  BOOST_CHECK_EQUAL(blockHeader.GetShardId(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetGasLimit(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetGasUsed(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetRewards(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetEpochNum(), (uint64_t)-1);
  BOOST_CHECK_EQUAL(blockHeader.GetTxRootHash(), TxnHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetStateDeltaHash(), StateHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetTranReceiptHash(), TxnHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetNumTxs(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetMinerPubKey(), PubKey{});
  BOOST_CHECK_EQUAL(blockHeader.GetDSBlockNum(), INIT_BLOCK_NUMBER);
  BOOST_CHECK_EQUAL(blockHeader.GetVersion(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetCommitteeHash(), CommitteeHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetPrevHash(), BlockHash{});
}

BOOST_AUTO_TEST_CASE(MicroBlockHeader_NonDefaultConstruction) {
  auto minerPubKey = PubKey::GetPubKeyFromString(
      "a0b54dfb242dbb7aabb5ab954e60125f4cfa12bc9aba5150f7c3012554d8de238a");

  MicroBlockHashSet hashSet{
      TxnHash{"4d99a6aad137aaad46f92e787f5d506e3249cf83cbbb9df23d38f049b986"
              "3205"},
      StateHash{"a32fab43af733e2735a82b196f5530eb67f193a2a6140a5adecdd5eb3e"
                "9f454a"},
      TxnHash{"99cba620b47e8bdad1a5aeb95d7b402c085074ee7c1724bd2936de10930b"
              "636c"},
  };
  MicroBlockHeader blockHeader{
      1,
      45,
      32,
      8,
      9122,
      hashSet,
      5,
      minerPubKey,
      172,
      1,  // version
      BlockHash(
          "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52"),
      BlockHash(
          "e21a8a7b4f014090eaffd3e64dac41dcea4f5f7bbe67e0ac4deeb9f975130b87")};

  BOOST_CHECK_EQUAL(blockHeader.GetShardId(), 1);
  BOOST_CHECK_EQUAL(blockHeader.GetGasLimit(), 45);
  BOOST_CHECK_EQUAL(blockHeader.GetGasUsed(), 32);
  BOOST_CHECK_EQUAL(blockHeader.GetRewards(), 8);
  BOOST_CHECK_EQUAL(blockHeader.GetEpochNum(), 9122);
  BOOST_CHECK_EQUAL(blockHeader.GetNumTxs(), 5);
  BOOST_CHECK_EQUAL(blockHeader.GetMinerPubKey(), minerPubKey);
  BOOST_CHECK_EQUAL(blockHeader.GetDSBlockNum(), 172);
  BOOST_CHECK_EQUAL(blockHeader.GetTxRootHash(), hashSet.m_txRootHash);
  BOOST_CHECK_EQUAL(blockHeader.GetStateDeltaHash(), hashSet.m_stateDeltaHash);
  BOOST_CHECK_EQUAL(blockHeader.GetTranReceiptHash(),
                    hashSet.m_tranReceiptHash);
}

BOOST_AUTO_TEST_CASE(MicroBlockHeader_CompareEqual) {
  auto minerPubKey = PubKey::GetPubKeyFromString(
      "9ab33a3868993176b613738816247a7f4d357cae555996519cf5b543e9b3554b89");

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
      5,
      minerPubKey,
      172,
      1,  // version
      BlockHash(
          "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52"),
      BlockHash(
          "e21a8a7b4f014090eaffd3e64dac41dcea4f5f7bbe67e0ac4deeb9f975130b87")};

  auto blockHeader2 = blockHeader1;

  BOOST_CHECK_EQUAL(blockHeader1, blockHeader2);
  BOOST_CHECK_EQUAL(blockHeader2, blockHeader1);

  BOOST_CHECK_EQUAL(MicroBlockHeader{}, MicroBlockHeader{});
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10,  70,  8,   1,   18,  32,  139, 125, 241, 67,  217, 28,  113, 110,
       207, 165, 252, 23,  48,  2,   47,  107, 66,  27,  5,   206, 222, 232,
       253, 82,  177, 252, 101, 169, 96,  48,  173, 82,  26,  32,  226, 26,
       138, 123, 79,  1,   64,  144, 234, 255, 211, 230, 77,  172, 65,  220,
       234, 79,  95,  123, 190, 103, 224, 172, 77,  238, 185, 249, 117, 19,
       11,  135, 16,  1,   24,  9,   32,  8,   42,  18,  10,  16,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   7,
       56,  141, 106, 66,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   72,  4,   82,  35,  10,
       33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   88,  130, 45,  98,  32,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   106, 32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0},
      {10,  70,  8,   1,   18,  32,  139, 125, 241, 67,  217, 28,  113, 110,
       207, 165, 252, 23,  48,  2,   47,  107, 66,  27,  5,   206, 222, 232,
       253, 82,  177, 252, 101, 169, 96,  48,  173, 82,  26,  32,  226, 26,
       138, 123, 79,  1,   64,  144, 234, 255, 211, 230, 77,  172, 65,  220,
       234, 79,  95,  123, 190, 103, 224, 172, 77,  238, 185, 249, 117, 19,
       11,  135, 16,  2,   24,  18,  32,  16,  42,  18,  10,  16,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   14,
       56,  142, 106, 66,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   72,  5,   82,  35,  10,
       33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   88,  131, 45,  98,  32,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   106, 32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0},
      {10,  70,  8,   1,   18,  32,  139, 125, 241, 67,  217, 28,  113, 110,
       207, 165, 252, 23,  48,  2,   47,  107, 66,  27,  5,   206, 222, 232,
       253, 82,  177, 252, 101, 169, 96,  48,  173, 82,  26,  32,  226, 26,
       138, 123, 79,  1,   64,  144, 234, 255, 211, 230, 77,  172, 65,  220,
       234, 79,  95,  123, 190, 103, 224, 172, 77,  238, 185, 249, 117, 19,
       11,  135, 16,  3,   24,  27,  32,  24,  42,  18,  10,  16,  0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   21,
       56,  143, 106, 66,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   72,  6,   82,  35,  10,
       33,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   88,  132, 45,  98,  32,  0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   106, 32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0}};

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
        static_cast<uint32_t>(i + 3),
        PubKey::GetPubKeyFromString(
            std::string(66, static_cast<char>('1' + i))),
        static_cast<uint64_t>(i + 5761),
        1,  // version
        BlockHash(
            "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52"),
        BlockHash("e21a8a7b4f014090eaffd3e64dac41dcea4f5f7bbe67e0ac4deeb9f97513"
                  "0b87")};

    BOOST_CHECK(blockHeader.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

    MicroBlockHeader deserializedBlockHeader;
    deserializedBlockHeader.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlockHeader.Deserialize(dst, 0));

    BOOST_CHECK(blockHeader == deserializedBlockHeader);
  }
}

BOOST_AUTO_TEST_SUITE_END()
