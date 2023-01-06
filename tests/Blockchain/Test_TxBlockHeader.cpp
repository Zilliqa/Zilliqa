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

#include "libData/BlockData/BlockHeader/TxBlockHeader.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#include <fstream>

#define BOOST_TEST_MODULE txblockheadertest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(txblockheadertest)

BOOST_AUTO_TEST_CASE(TxBlockHeader_DefaultConstruction) {
  TxBlockHeader blockHeader;

  BOOST_CHECK_EQUAL(blockHeader.GetGasLimit(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetGasUsed(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetRewards(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetBlockNum(), INIT_BLOCK_NUMBER);
  BOOST_CHECK_EQUAL(blockHeader.GetNumTxs(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetMinerPubKey(), PubKey{});
  BOOST_CHECK_EQUAL(blockHeader.GetDSBlockNum(), INIT_BLOCK_NUMBER);
  BOOST_CHECK_EQUAL(blockHeader.GetStateRootHash(), StateHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetStateDeltaHash(), StateHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetVersion(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetCommitteeHash(), CommitteeHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetPrevHash(), BlockHash{});
}

BOOST_AUTO_TEST_CASE(TxBlockHeader_NonDefaultConstruction) {
  auto minerPubKey = PubKey::GetPubKeyFromString(
      "8b133a3868993176b613738816247a7f4d357cae555996519cf5b543e9b3554b89");
  TxBlockHeader blockHeader{
      54,
      23,
      3,
      1235,
      {},
      9,
      minerPubKey,
      211,
      1,  // version
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131"),
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61")};

  BOOST_CHECK_EQUAL(blockHeader.GetGasLimit(), 54);
  BOOST_CHECK_EQUAL(blockHeader.GetGasUsed(), 23);
  BOOST_CHECK_EQUAL(blockHeader.GetRewards(), 3);
  BOOST_CHECK_EQUAL(blockHeader.GetBlockNum(), 1235);
  BOOST_CHECK_EQUAL(blockHeader.GetNumTxs(), 9);
  BOOST_CHECK_EQUAL(blockHeader.GetMinerPubKey(), minerPubKey);
  BOOST_CHECK_EQUAL(blockHeader.GetDSBlockNum(), 211);
  BOOST_CHECK_EQUAL(blockHeader.GetStateRootHash(), StateHash{});
  BOOST_CHECK_EQUAL(blockHeader.GetStateDeltaHash(), StateHash{});
}

BOOST_AUTO_TEST_CASE(TxBlockHeader_CompareEqual) {

  auto minerPubKey = PubKey::GetPubKeyFromString(
      "9ab33a3868993176b613738816247a7f4d357cae555996519cf5b543e9b3554b89");
  TxBlockHeader blockHeader1{
      5,
      2,
      0,
      235,
      {},
      8,
      minerPubKey,
      11,
      1,  // version
      BlockHash(
          "9123dcbb0b42652b0e105956c68d3ca2ff34584f324fa41a29aedd32b883e131"),
      BlockHash(
          "717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61")};

  auto blockHeader2 = blockHeader1;

  BOOST_CHECK_EQUAL(blockHeader1, blockHeader2);
  BOOST_CHECK_EQUAL(blockHeader2, blockHeader1);

  BOOST_CHECK_EQUAL(TxBlockHeader{}, TxBlockHeader{});
}

BOOST_AUTO_TEST_CASE(Test_Serialization) {
#if 0
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
#endif
  for (int i = 1; i <= 3; ++i) {
    zbytes dst;

    TxBlockHeader blockHeader{
        static_cast<uint64_t>(i * 9),
        static_cast<uint64_t>(i * 8),
        static_cast<unsigned char>(i * 7),
        static_cast<uint64_t>(i * 10),
        {},
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
#if 0
    BOOST_TEST(dst == serialized[i - 1]);

    std::ofstream file{"dst-" + std::to_string(i)};
    file << '{';
    for (auto v : dst) file << static_cast<unsigned int>(v) << ", ";
    file << '}';
#endif

    TxBlockHeader deserializedBlockHeader;
    deserializedBlockHeader.Deserialize(dst, 0);
    BOOST_CHECK(deserializedBlockHeader.Deserialize(dst, 0));

    BOOST_CHECK(blockHeader == deserializedBlockHeader);
  }
}

BOOST_AUTO_TEST_SUITE_END()
