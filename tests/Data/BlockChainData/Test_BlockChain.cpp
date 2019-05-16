/*
 * Copyright (C) 2019 Zilliqa
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

#include <array>
#include <string>
#include <vector>

#include "libCrypto/Sha2.h"
#include "libData/BlockChainData/BlockChain.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE blockchaintest
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_ALTERNATIVE_INIT_API

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(blockchaintest)

// Fills vector of BlockLink starting at position _from_ with random number of
// BlockLink elements in range <_min_,_max_ >. Maximum is size(uint8_t).
void appendBlockLinkAndChain_v(BlockLinkChain& blc, vector<BlockLink>& bl_v,
                               const uint8_t& min, const uint8_t& max) {
  if (max < min) {
    string throw_s =
        "Invalid range, max " + to_string(max) + " <  min" + to_string(min);
    throw throw_s;
  }

  uint8_t lastIndex = bl_v.size();
  uint8_t lastIndex_new =
      lastIndex + TestUtils::RandomIntInRng<uint8_t>(min, max);

  for (uint16_t i = lastIndex; i < lastIndex_new; i++) {
    uint64_t dsindex = TestUtils::DistUint64();
    BlockType blocktype =
        static_cast<BlockType>(TestUtils::RandomIntInRng<unsigned char>(0, 4));
    BlockHash blockhash = BlockHash::random();
    bl_v.push_back(
        make_tuple(BLOCKLINK_VERSION, i, dsindex, blocktype, blockhash));
    BOOST_CHECK_MESSAGE(blc.AddBlockLink(i, dsindex, blocktype, blockhash),
                        "Cannot add block link\n");
  }
}

bool operator==(const BlockLink& c1, const BlockLink& c2) {
  return (get<0>(c1) == get<0>(c2) && get<1>(c1) == get<1>(c2) &&
          get<2>(c1) == get<2>(c2) && get<3>(c1) == get<3>(c2) &&
          get<4>(c1) == get<4>(c2));
}

BOOST_AUTO_TEST_CASE(BlockLinkChain_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockLinkChain blc;

  vector<BlockLink> blTest_v;
  uint64_t blocknum = 1;
  // Add first BlockLink with index higher than 0
  BOOST_CHECK_MESSAGE(blc.AddBlockLink(1, 1, DS, BlockHash::random()) == false,
                      "Can add first BlockLink with index greater than zero " +
                          to_string(blocknum) + ".\n");

  appendBlockLinkAndChain_v(blc, blTest_v, 1, BLOCKCHAIN_SIZE);

  // Get and compare added random BlockLink
  uint8_t randBlockLinkIndex =
      TestUtils::RandomIntInRng<uint16_t>(0, blTest_v.size() - 1);
  BOOST_CHECK_MESSAGE(
      blc.GetBlockLink(randBlockLinkIndex) == blTest_v[randBlockLinkIndex],
      "BlockLink in BlockLinkChain not equals to added one.\n");

  // Access out of index
  uint64_t index_out = blTest_v.size();
  BlockLink empty_blc;
  BOOST_CHECK_MESSAGE(
      empty_blc == blc.GetBlockLink(index_out),
      "Empty BlockLinkChain had to be returned when accessed out of index " +
          to_string(index_out) + ".\n");

  // Get BlockLink from persistent storage - BLOCKCHAIN_SIZE exceeded and
  // element on index > BLOCKCHAIN_SIZE will be accessed
  appendBlockLinkAndChain_v(blc, blTest_v, BLOCKCHAIN_SIZE - blTest_v.size(),
                            BLOCKCHAIN_SIZE + 10);
  BOOST_CHECK_MESSAGE(
      blTest_v[1] == blc.GetBlockLink(1),
      "Incorrect BlockLink returned from persistent storage.\n");

  // Get latest BlockLink
  BOOST_CHECK_MESSAGE(blc.GetLatestBlockLink() == blTest_v.back(),
                      "Incorrect latest BlockLink returned.\n");

  // Set and get DSComm
  PubKey pk_in = TestUtils::GenerateRandomPubKey();
  Peer peer_in = TestUtils::GenerateRandomPeer();
  std::deque<std::pair<PubKey, Peer>> dq_in = {make_pair(pk_in, peer_in)};
  blc.SetBuiltDSComm(dq_in);
  std::deque<std::pair<PubKey, Peer>> dq_out;

  dq_out = blc.GetBuiltDSComm();
  PubKey pk_out = dq_out.back().first;
  Peer peer_out = dq_out.back().second;
  BOOST_CHECK_MESSAGE(pk_in == pk_out && peer_in == peer_out,
                      "DSComm obtained not equal to the set one.\n");

  // Add BlockLink with lower index than the latest
  uint64_t index_old = 1;
  BOOST_CHECK_MESSAGE(
      blc.AddBlockLink(index_old, 1, DS, BlockHash::random()) == false,
      "Can add BlockLink with index " + to_string(index_old) +
          " lower then the latest index " + to_string(blc.GetLatestIndex()) +
          ".\n");
}

template <class T1, class T2>
void test_BlockChain(T1& blockChain, T2& block_0, T2& block_1, T2& block_last,
                     T2& block_empty) {
  BOOST_CHECK_MESSAGE(blockChain.AddBlock(block_0) == 1,
                      "Unable to add block.\n");
  BOOST_CHECK_MESSAGE(blockChain.GetBlock(0) == block_0,
                      "Incorrect BlockCount " +
                          to_string(blockChain.GetBlockCount()) +
                          " != " + to_string(1) + ".\n");
  BOOST_CHECK_MESSAGE(blockChain.GetBlock(1) == block_empty,
                      "Nonempty block returned when getting on index where no "
                      "add done before.\n");
  BOOST_CHECK_MESSAGE(blockChain.AddBlock(block_1) == 1,
                      "Unable to add block.\n");
  BOOST_CHECK_MESSAGE(blockChain.AddBlock(block_last) == 1,
                      "Unable to add block.\n");
  BOOST_CHECK_MESSAGE(
      blockChain.AddBlock(block_0) == -1,
      "Can add block with header number " +
          to_string(block_0.GetHeader().GetBlockNum()) +
          " lover than in the last "
          " added header " +
          to_string(blockChain.GetLastBlock().GetHeader().GetBlockNum() - 1) +
          ".\n");
  // Causes segfault since BlockStorage is empty
  uint64_t blocknum_overwritten = 0;
  BOOST_CHECK_MESSAGE(blockChain.GetBlock(blocknum_overwritten) == block_empty,
                      "Nonempty block returned when queried block number " +
                          to_string(blocknum_overwritten) +
                          " already overwritten by block number " +
                          to_string(blocknum_overwritten +
                                    block_last.GetHeader().GetBlockNum()) +
                          ".\n");
  BOOST_CHECK_MESSAGE(blockChain.GetBlockCount() == BLOCKCHAIN_SIZE + 1,
                      "Incorrect BlockCount " +
                          to_string(blockChain.GetBlockCount()) +
                          " != " + to_string(1) + ".\n");
  BOOST_CHECK_MESSAGE(
      blockChain.GetLastBlock() == block_last,
      "GetLastBlock returned block different from block added last.\n");
}

BOOST_AUTO_TEST_CASE(DSBlockChain_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DSBlockChain dsbc;
  // Trying access undefined block
  DSBlock dsb_empty;
  uint64_t blocknum_rand =
      TestUtils::RandomIntInRng<uint8_t>(0, BLOCKCHAIN_SIZE);
  BOOST_CHECK_MESSAGE(dsb_empty == dsbc.GetBlock(blocknum_rand),
                      "DSBlockChain didn't return dummy block when not yet "
                      "added block number " +
                          to_string(blocknum_rand) + " accessed.\n");
  uint64_t block_count = dsbc.GetBlockCount();
  BOOST_CHECK_MESSAGE(block_count == 0,
                      "DSBlockChain returned blockCount not equal to zero "
                      "after construction " +
                          to_string(block_count) + ".\n");
  DSBlock dsb_0(TestUtils::createDSBlockHeader(0), CoSignatures());
  DSBlock dsb_1(TestUtils::createDSBlockHeader(1), CoSignatures());
  DSBlock lastBlock =
      DSBlock(TestUtils::createDSBlockHeader(BLOCKCHAIN_SIZE), CoSignatures());

  test_BlockChain(dsbc, dsb_0, dsb_1, lastBlock, dsb_empty);
}

BOOST_AUTO_TEST_CASE(TxBlockChain_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TxBlockChain txbc;
  // Trying access undefined block
  TxBlock txb_empty;
  uint64_t blocknum_rand =
      TestUtils::RandomIntInRng<uint8_t>(0, BLOCKCHAIN_SIZE);
  BOOST_CHECK_MESSAGE(txb_empty == txbc.GetBlock(blocknum_rand),
                      "DSBlockChain didn't return dummy block when not yet "
                      "added block number " +
                          to_string(blocknum_rand) + " accessed.\n");
  uint64_t block_count = txbc.GetBlockCount();
  BOOST_CHECK_MESSAGE(txbc.GetBlockCount() == 0,
                      "DSBlockChain returned blockCount not equal to zero "
                      "after construction " +
                          to_string(block_count) + ".\n");

  TxBlock txb_0(TestUtils::createTxBlockHeader(0),
                std::vector<MicroBlockInfo>(), CoSignatures());
  TxBlock txb_1(TestUtils::createTxBlockHeader(1),
                std::vector<MicroBlockInfo>(), CoSignatures());
  TxBlock lastBlock = TxBlock(TestUtils::createTxBlockHeader(BLOCKCHAIN_SIZE),
                              std::vector<MicroBlockInfo>(), CoSignatures());

  test_BlockChain(txbc, txb_0, txb_1, lastBlock, txb_empty);
}

BOOST_AUTO_TEST_SUITE_END()
