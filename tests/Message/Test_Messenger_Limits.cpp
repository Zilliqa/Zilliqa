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

#include <limits>
#include <random>
#include "libMessage/Messenger.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(messenger_limits_test)

BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  TestUtils::Initialize();
}

BOOST_AUTO_TEST_CASE(test_GetLookupSetTxBlockFromSeed) {
  bytes dst;
  const unsigned int offset = 0;
  const uint64_t lowBlockNum = TestUtils::DistUint64();
  const uint64_t highBlockNum = TestUtils::DistUint64();
  const PairOfKey lookupKey = TestUtils::GenerateRandomKeyPair();

  // Create a dummy TxBlock
  const TxBlock txBlock(TestUtils::GenerateRandomTxBlockHeader(),
                        vector<MicroBlockInfo>{},
                        TestUtils::GenerateRandomCoSignatures());

  // Get the approximate size each TxBlock adds to a SETTXBLOCKFROMSEED message
  vector<TxBlock> txBlocks;
  txBlocks.emplace_back(txBlock);
  BOOST_CHECK(Messenger::SetLookupSetTxBlockFromSeed(
      dst, offset, lowBlockNum, highBlockNum, lookupKey, txBlocks));
  const unsigned int sizeWithOneBlock = dst.size();
  dst.clear();
  txBlocks.emplace_back(txBlock);
  BOOST_CHECK(Messenger::SetLookupSetTxBlockFromSeed(
      dst, offset, lowBlockNum, highBlockNum, lookupKey, txBlocks));
  const unsigned int sizeWithTwoBlocks = dst.size();
  dst.clear();
  const unsigned int sizePerBlock = sizeWithTwoBlocks - sizeWithOneBlock;

  // Compute how much to reach the limit MAX_READ_WATERMARK_IN_BYTES
  unsigned int numBlocksToReachLimit =
      (MAX_READ_WATERMARK_IN_BYTES - sizeWithTwoBlocks) / sizePerBlock;
  if (sizeWithTwoBlocks + (numBlocksToReachLimit * sizePerBlock) >=
      MAX_READ_WATERMARK_IN_BYTES) {
    numBlocksToReachLimit--;
  }

  // Add the blocks to the list
  for (unsigned int i = 0; i < numBlocksToReachLimit; i++) {
    txBlocks.emplace_back(txBlock);
  }

  // Test for just below the limit
  BOOST_CHECK(Messenger::SetLookupSetTxBlockFromSeed(
      dst, offset, lowBlockNum, highBlockNum, lookupKey, txBlocks));
  uint64_t lowBlockNumDeserialized = 0;
  uint64_t highBlockNumDeserialized = 0;
  PubKey lookupPubKeyDeserialized;
  vector<TxBlock> txBlocksDeserialized;
  BOOST_CHECK(Messenger::GetLookupSetTxBlockFromSeed(
      dst, offset, lowBlockNumDeserialized, highBlockNumDeserialized,
      lookupPubKeyDeserialized, txBlocksDeserialized));
  BOOST_CHECK(lowBlockNum == lowBlockNumDeserialized);
  BOOST_CHECK(highBlockNum == highBlockNumDeserialized);
  BOOST_CHECK(lookupKey.second == lookupPubKeyDeserialized);
  BOOST_CHECK(txBlocks == txBlocksDeserialized);

  // Test for above the limit. Let's add a few just to be sure.
  for (unsigned int i = 0; i < 10; i++) {
    txBlocks.emplace_back(txBlock);
  }
  dst.clear();
  BOOST_CHECK(Messenger::SetLookupSetTxBlockFromSeed(
      dst, offset, lowBlockNum, highBlockNum, lookupKey, txBlocks));
  BOOST_CHECK(!Messenger::GetLookupSetTxBlockFromSeed(
      dst, offset, lowBlockNumDeserialized, highBlockNumDeserialized,
      lookupPubKeyDeserialized, txBlocksDeserialized));
}

BOOST_AUTO_TEST_CASE(test_GetLookupSetDirectoryBlocksFromSeed) {
  bytes dst;
  const unsigned int offset = 0;
  const uint32_t shardingStructureVersion = TestUtils::DistUint32();
  const uint64_t indexNum = TestUtils::DistUint64();
  const PairOfKey lookupKey = TestUtils::GenerateRandomKeyPair();

  // Create a dummy DSBlock
  DSBlock dsBlock(TestUtils::GenerateRandomDSBlockHeader(),
                  TestUtils::GenerateRandomCoSignatures());

  // Get the approximate size each DSBlock adds to a SETDIRBLOCKSFROMSEED
  // message
  vector<boost::variant<DSBlock, VCBlock>> directoryBlocks;
  directoryBlocks.emplace_back(dsBlock);
  BOOST_CHECK(Messenger::SetLookupSetDirectoryBlocksFromSeed(
      dst, offset, shardingStructureVersion, directoryBlocks, indexNum,
      lookupKey));
  const unsigned int sizeWithOneBlock = dst.size();
  dst.clear();
  directoryBlocks.emplace_back(dsBlock);
  BOOST_CHECK(Messenger::SetLookupSetDirectoryBlocksFromSeed(
      dst, offset, shardingStructureVersion, directoryBlocks, indexNum,
      lookupKey));
  const unsigned int sizeWithTwoBlocks = dst.size();
  dst.clear();
  const unsigned int sizePerBlock = sizeWithTwoBlocks - sizeWithOneBlock;

  // Compute how much to reach the limit MAX_READ_WATERMARK_IN_BYTES
  unsigned int numBlocksToReachLimit =
      (MAX_READ_WATERMARK_IN_BYTES - sizeWithTwoBlocks) / sizePerBlock;
  if (sizeWithTwoBlocks + (numBlocksToReachLimit * sizePerBlock) >=
      MAX_READ_WATERMARK_IN_BYTES) {
    numBlocksToReachLimit--;
  }

  // Add the blocks to the list
  for (unsigned int i = 0; i < numBlocksToReachLimit; i++) {
    directoryBlocks.emplace_back(dsBlock);
  }

  // Test for just below the limit
  BOOST_CHECK(Messenger::SetLookupSetDirectoryBlocksFromSeed(
      dst, offset, shardingStructureVersion, directoryBlocks, indexNum,
      lookupKey));
  uint32_t dummyShardingStructureVersionDeserialized = 0;  // Unchecked
  uint64_t indexNumDeserialized = 0;
  PubKey lookupPubKeyDeserialized;
  vector<boost::variant<DSBlock, VCBlock>> directoryBlocksDeserialized;
  BOOST_CHECK(Messenger::GetLookupSetDirectoryBlocksFromSeed(
      dst, offset, dummyShardingStructureVersionDeserialized,
      directoryBlocksDeserialized, indexNumDeserialized,
      lookupPubKeyDeserialized));
  BOOST_CHECK(directoryBlocks.size() == directoryBlocksDeserialized.size());
  for (unsigned int i = 0; i < directoryBlocks.size(); i++) {
    BOOST_CHECK(get<DSBlock>(directoryBlocks.at(i)) ==
                get<DSBlock>(directoryBlocksDeserialized.at(i)));
  }
  BOOST_CHECK(indexNum == indexNumDeserialized);
  BOOST_CHECK(lookupKey.second == lookupPubKeyDeserialized);

  // Test for above the limit. Let's add a few just to be sure.
  for (unsigned int i = 0; i < 10; i++) {
    directoryBlocks.emplace_back(dsBlock);
  }
  dst.clear();
  BOOST_CHECK(Messenger::SetLookupSetDirectoryBlocksFromSeed(
      dst, offset, shardingStructureVersion, directoryBlocks, indexNum,
      lookupKey));
  BOOST_CHECK(!Messenger::GetLookupSetDirectoryBlocksFromSeed(
      dst, offset, dummyShardingStructureVersionDeserialized,
      directoryBlocksDeserialized, indexNumDeserialized,
      lookupPubKeyDeserialized));
}

BOOST_AUTO_TEST_SUITE_END()
