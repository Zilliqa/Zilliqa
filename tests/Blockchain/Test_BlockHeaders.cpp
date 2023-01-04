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

#include "libData/BlockData/BlockHeader/DSBlockHeader.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE blockchainheaderstest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(blockchainheaderstest)

BOOST_AUTO_TEST_CASE(BlockChainHeaders_test) { LOG_MARKER(); }

BOOST_AUTO_TEST_CASE(DSBlockHeader_DefaultConstruction) {
  DSBlockHeader blockHeader;

  BOOST_CHECK_EQUAL(blockHeader.GetDSDifficulty(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetDifficulty(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetTotalDifficulty(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetLeaderPubKey(), PubKey{});
  BOOST_CHECK_EQUAL(blockHeader.GetBlockNum(), INIT_BLOCK_NUMBER);
  BOOST_CHECK_EQUAL(blockHeader.GetEpochNum(), -1);
  BOOST_CHECK_EQUAL(blockHeader.GetGasPrice(), 0);
  BOOST_CHECK_EQUAL(blockHeader.GetSWInfo(), SWInfo{});
  BOOST_CHECK(blockHeader.GetDSPoWWinners().empty());
  BOOST_CHECK(blockHeader.GetDSRemovePubKeys().empty());
  BOOST_CHECK(blockHeader.GetGovProposalMap().empty());
  BOOST_CHECK_EQUAL(blockHeader.GetShardingHash(), ShardingHash{});

  for (auto i : blockHeader.GetHashSetReservedField()) BOOST_CHECK_EQUAL(i, 0);
}

BOOST_AUTO_TEST_CASE(DSBlockHeader_NonDefaultConstruction) {
  auto key = PubKey::GetPubKeyFromString(
      "872e4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa");
  DSBlockHeader blockHeader{41, 92, key, 33, 89};

  BOOST_CHECK_EQUAL(blockHeader.GetDSDifficulty(), 41);
  BOOST_CHECK_EQUAL(blockHeader.GetDifficulty(), 92);
  BOOST_CHECK_EQUAL(blockHeader.GetLeaderPubKey(), key);
  BOOST_CHECK_EQUAL(blockHeader.GetBlockNum(), 33);
  BOOST_CHECK_EQUAL(blockHeader.GetEpochNum(), 89);
}

BOOST_AUTO_TEST_CASE(DSBlockHeader_CompareEqual) {
  auto key = PubKey::GetPubKeyFromString(
      "9fff4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa");
  DSBlockHeader blockHeader1{
      9,
      2,
      key,
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

  auto blockHeader2 = blockHeader1;

  BOOST_CHECK_EQUAL(blockHeader1, blockHeader2);
  BOOST_CHECK_EQUAL(blockHeader2, blockHeader1);

  BOOST_CHECK_EQUAL(DSBlockHeader{}, DSBlockHeader{});
}

BOOST_AUTO_TEST_CASE(DSBlockHeader_CompareGreaterLessThan) {
  DSBlockHeader blockHeader1{4, 2, PubKey{}, 9, 10};
  DSBlockHeader blockHeader2{4, 2, PubKey{}, 10, 10};
  DSBlockHeader blockHeader3{4, 2, PubKey{}, 11, 3};
  DSBlockHeader blockHeader4{2, 2, PubKey{}, 12, 2};
  DSBlockHeader blockHeader5{1, 2, PubKey{}, 99, 1};

  BOOST_CHECK_LT(blockHeader1, blockHeader2);
  BOOST_CHECK_GT(blockHeader2, blockHeader1);

  BOOST_CHECK_LT(blockHeader2, blockHeader3);
  BOOST_CHECK_GT(blockHeader3, blockHeader2);

  BOOST_CHECK_LT(blockHeader3, blockHeader4);
  BOOST_CHECK_GT(blockHeader4, blockHeader3);

  BOOST_CHECK_LT(blockHeader4, blockHeader5);
  BOOST_CHECK_GT(blockHeader5, blockHeader4);

  BOOST_CHECK_LT(blockHeader1, blockHeader5);
  BOOST_CHECK_GT(blockHeader5, blockHeader1);
}

BOOST_AUTO_TEST_CASE(DSBlockHeader_GetHashForRandom) {
  DSBlockHeader blockHeader1{
      111, 4,
      PubKey::GetPubKeyFromString(
          "9fff4e50ce9990d8b041330c47c9ddd11bec6b503ae9386a99da8584e9bb12c4aa"),
      999, 888};

  BOOST_CHECK_EQUAL(
      blockHeader1.GetHashForRandom(),
      BlockHash(
          "9aa9a8d44726c8a34ed364acdb498b1fb80296a35d26320821fa2ae1d4851052",
          BlockHash::ConstructFromStringType::FromHex));

  DSBlockHeader blockHeader2{
      9,
      123,
      PubKey::GetPubKeyFromString(
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      5,
      11,
      810,
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
          "c22b1ab817891c54a3e3c2bb1e1e09a9a616cb2a763f8027cd8646ec1ee038e6"),
      BlockHash(
          "677dc8f0cbe535e8ee53ea9bb8a0f2517857bc827fe8aed9aba734d8d5d2f282")};

  BOOST_CHECK_EQUAL(
      blockHeader2.GetHashForRandom(),
      BlockHash(
          "4611757dda494c9ed95de4c47877221187587860cf105fbb80b927f1de3237aa",
          BlockHash::ConstructFromStringType::FromHex));
}

BOOST_AUTO_TEST_SUITE_END()
