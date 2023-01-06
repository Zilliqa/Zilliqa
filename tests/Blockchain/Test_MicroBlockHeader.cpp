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

#include <fstream>

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
  BOOST_CHECK_EQUAL(blockHeader.GetDSBlockNum(), 0);
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

#if 0
BOOST_AUTO_TEST_CASE(Test_Serialization) {
  zbytes serialized[3] = {
      {10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149, 13,  160, 204,
       182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162, 12,  188, 162,
       124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,  32,  145, 35,
       220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198, 141, 60,  162,
       255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221, 50,  184, 131,
       225, 49,  16,  9,   24,  8,   34,  18,  10,  16,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   7,   48,  10,
       58,  102, 10,  32,  207, 116, 107, 11,  134, 184, 80,  247, 113, 178,
       117, 58,  163, 187, 245, 108, 214, 2,   222, 1,   31,  29,  74,  79,
       36,  173, 101, 63,  183, 210, 73,  150, 18,  32,  255, 124, 156, 249,
       165, 117, 10,  25,  41,  116, 214, 226, 9,   148, 77,  78,  168, 107,
       163, 90,  106, 41,  173, 148, 112, 93,  134, 182, 79,  229, 204, 203,
       26,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   64,  136, 1,   74,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   80,  20},
      {10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149, 13,  160, 204,
       182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162, 12,  188, 162,
       124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,  32,  145, 35,
       220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198, 141, 60,  162,
       255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221, 50,  184, 131,
       225, 49,  16,  18,  24,  16,  34,  18,  10,  16,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   14,  48,  20,
       58,  102, 10,  32,  207, 116, 107, 11,  134, 184, 80,  247, 113, 178,
       117, 58,  163, 187, 245, 108, 214, 2,   222, 1,   31,  29,  74,  79,
       36,  173, 101, 63,  183, 210, 73,  150, 18,  32,  255, 124, 156, 249,
       165, 117, 10,  25,  41,  116, 214, 226, 9,   148, 77,  78,  168, 107,
       163, 90,  106, 41,  173, 148, 112, 93,  134, 182, 79,  229, 204, 203,
       26,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   64,  137, 1,   74,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   80,  21},
      {10,  70,  8,   1,   18,  32,  113, 122, 197, 6,   149, 13,  160, 204,
       182, 64,  76,  221, 94,  117, 145, 247, 32,  24,  162, 12,  188, 162,
       124, 138, 66,  62,  156, 158, 86,  38,  172, 97,  26,  32,  145, 35,
       220, 187, 11,  66,  101, 43,  14,  16,  89,  86,  198, 141, 60,  162,
       255, 52,  88,  79,  50,  79,  164, 26,  41,  174, 221, 50,  184, 131,
       225, 49,  16,  27,  24,  24,  34,  18,  10,  16,  0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   21,  48,  30,
       58,  102, 10,  32,  207, 116, 107, 11,  134, 184, 80,  247, 113, 178,
       117, 58,  163, 187, 245, 108, 214, 2,   222, 1,   31,  29,  74,  79,
       36,  173, 101, 63,  183, 210, 73,  150, 18,  32,  255, 124, 156, 249,
       165, 117, 10,  25,  41,  116, 214, 226, 9,   148, 77,  78,  168, 107,
       163, 90,  106, 41,  173, 148, 112, 93,  134, 182, 79,  229, 204, 203,
       26,  32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   64,  138, 1,   74,  35,  10,  33,  0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   80,  22}};

  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

    MicroBlockHeader blockHeader{
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

    BOOST_CHECK(blockHeader.Serialize(dst, 0));
    BOOST_TEST(dst == serialized[i - 1]);

#if 0
    std::ofstream file{"dst-" + std::to_string(i)};
    file << '{';
    for (auto v : dst) file << static_cast<unsigned int>(v) << ", ";
    file << '}';
#endif

    MicroBlockHeader deserializedBlockHeader;
    deserializedBlockHeader.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlockHeader.Deserialize(dst, 0));

    BOOST_CHECK(blockHeader == deserializedBlockHeader);
  }
}
#endif

BOOST_AUTO_TEST_SUITE_END()
