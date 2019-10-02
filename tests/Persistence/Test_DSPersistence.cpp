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
#include <thread>
#include <vector>

#include "libData/BlockData/Block.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testReadWriteSimpleStringToDB) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DB db("test.db");

  db.WriteToDB("fruit", "vegetable");

  std::string ret = db.ReadFromDB("fruit");

  BOOST_CHECK_MESSAGE(
      ret == "vegetable",
      "ERROR: return value from DB not equal to inserted value");
}

DSBlock constructDummyDSBlock(uint64_t blocknum) {
  BlockHash prevHash1;

  for (unsigned int i = 0; i < prevHash1.asArray().size(); i++) {
    prevHash1.asArray().at(i) = i + 1;
  }

  PairOfKey pubKey1 = Schnorr::GenKeyPair();

  std::map<PubKey, Peer> powDSWinners;
  for (int i = 0; i < 3; i++) {
    powDSWinners[Schnorr::GenKeyPair().second] = Peer();
  }

  std::vector<PubKey> removeDSNodePubkeys;
  removeDSNodePubkeys.reserve(2);
  for (int i = 0; i < 2; i++) {
    removeDSNodePubkeys.emplace_back(Schnorr::GenKeyPair().second);
  }

  return DSBlock(DSBlockHeader(50, 20, pubKey1.second, blocknum, 0,
                               PRECISION_MIN_VALUE, SWInfo(), powDSWinners,
                               removeDSNodePubkeys, DSBlockHashSet(),
                               DSBLOCK_VERSION, CommitteeHash(), prevHash1),
                 CoSignatures());
}

BOOST_AUTO_TEST_CASE(testSerializationDeserialization) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // checking if normal serialization and deserialization of blocks is working
  // or not

  DSBlock block1 = constructDummyDSBlock(0);

  bytes serializedDSBlock;
  block1.Serialize(serializedDSBlock, 0);

  DSBlock block2(serializedDSBlock, 0);

  BOOST_CHECK_MESSAGE(
      block2.GetHeader().GetBlockNum() == block1.GetHeader().GetBlockNum(),
      "block num shouldn't change after serailization and deserialization");
}

BOOST_AUTO_TEST_CASE(testBlockStorage) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DSBlock block1 = constructDummyDSBlock(0);

  bytes serializedDSBlock;
  block1.Serialize(serializedDSBlock, 0);

  BlockStorage::GetBlockStorage().PutDSBlock(0, serializedDSBlock);

  DSBlockSharedPtr block2;
  BlockStorage::GetBlockStorage().GetDSBlock(0, block2);

  // using individual == tests instead of DSBlockHeader::operator== to zero in
  // which particular data type fails on writing to/ reading from disk

  // using individual == tests instead of DSBlockHeader::operator== to zero in
  // // using individual == tests instead of DSBlockHeader::operator== to zero
  // in which particular data type fails on writing to/ reading from disk
  // // which particular data type fails on writing to/ reading from disk
  LOG_GENERAL(INFO,
              "Block1 num value entered: " << block1.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block2 num value retrieved: "
                        << (*block2).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetBlockNum() == (*block2).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");

  LOG_GENERAL(INFO, "Block1 difficulty value entered: "
                        << (int)(block1.GetHeader().GetDifficulty()));
  LOG_GENERAL(INFO, "Block2 difficulty value retrieved: "
                        << (int)((*block2).GetHeader().GetDifficulty()));
  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetDifficulty() ==
          (*block2).GetHeader().GetDifficulty(),
      "difficulty shouldn't change after writing to/ reading from disk");

  LOG_GENERAL(INFO, "Block1 blocknum value entered: "
                        << block1.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block2 blocknum value retrieved: "
                        << (*block2).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetBlockNum() == (*block2).GetHeader().GetBlockNum(),
      "blocknum shouldn't change after writing to/ reading from disk");

  LOG_GENERAL(INFO,
              "Block1 timestamp value entered: " << block1.GetTimestamp());
  LOG_GENERAL(INFO,
              "Block2 timestamp value retrieved: " << (*block2).GetTimestamp());
  BOOST_CHECK_MESSAGE(
      block1.GetTimestamp() == (*block2).GetTimestamp(),
      "timestamp shouldn't change after writing to/ reading from disk");

  // LOG_GENERAL(INFO, "Block1 LeaderPubKey value entered: " <<
  // block1.GetHeader().GetLeaderPubKey()); LOG_GENERAL(INFO, "Block2
  // LeaderPubKey value retrieved: " <<
  // (*block2).GetHeader().GetLeaderPubKey());
  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetLeaderPubKey() ==
          (*block2).GetHeader().GetLeaderPubKey(),
      "LeaderPubKey shouldn't change after writing to/ reading from disk");

  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetPrevHash() == (*block2).GetHeader().GetPrevHash(),
      "PrevHash shouldn't change after writing to/ reading from disk");

  LOG_PAYLOAD(WARNING, "serializedDSBlock", serializedDSBlock,
              serializedDSBlock.size());

  bytes serializedDSBlock2;
  (*block2).Serialize(serializedDSBlock2, 0);
  LOG_PAYLOAD(WARNING, "serializedDSBlock2", serializedDSBlock2,
              serializedDSBlock2.size());

  std::string cs1, cs2;
  DataConversion::SerializableToHexStr(block1.GetCS2(), cs1);
  DataConversion::SerializableToHexStr((*block2).GetCS2(), cs2);
  BOOST_CHECK_MESSAGE(
      block1.GetCS2() == (*block2).GetCS2(),
      "Signature shouldn't change after writing to/ reading from disk. Orig: "
      "0x" << cs1
           << " out: 0x" << cs2);
}

BOOST_AUTO_TEST_CASE(testRandomBlockAccesses) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DSBlock block1 = constructDummyDSBlock(1);
  DSBlock block2 = constructDummyDSBlock(2);
  DSBlock block3 = constructDummyDSBlock(3);
  DSBlock block4 = constructDummyDSBlock(4);

  bytes serializedDSBlock;

  block1.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(1, serializedDSBlock);

  serializedDSBlock.clear();
  block2.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(2, serializedDSBlock);

  serializedDSBlock.clear();
  block3.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(3, serializedDSBlock);

  serializedDSBlock.clear();
  block4.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(4, serializedDSBlock);

  DSBlockSharedPtr blockRetrieved;
  BlockStorage::GetBlockStorage().GetDSBlock(2, blockRetrieved);

  LOG_GENERAL(INFO,
              "Block num value entered: " << block2.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block num value retrieved: "
                        << (*blockRetrieved).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block2.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "num shouldn't change after writing to/ reading from disk");

  BlockStorage::GetBlockStorage().GetDSBlock(4, blockRetrieved);
  LOG_GENERAL(INFO,
              "Block num value entered: " << block4.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block num value retrieved: "
                        << (*blockRetrieved).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block4.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk. Orig: "
          << block4.GetHeader().GetBlockNum()
          << " Out: " << (*blockRetrieved).GetHeader().GetBlockNum());

  BlockStorage::GetBlockStorage().GetDSBlock(1, blockRetrieved);
  LOG_GENERAL(INFO,
              "Block num value entered: " << block1.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block num value retrieved: "
                        << (*blockRetrieved).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");
}

BOOST_AUTO_TEST_CASE(testCachedAndEvictedBlocks) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DSBlock block = constructDummyDSBlock(1);

  for (int i = 5; i < 21; i++) {
    block = constructDummyDSBlock(i);

    bytes serializedDSBlock;

    block.Serialize(serializedDSBlock, 0);
    BlockStorage::GetBlockStorage().PutDSBlock(i, serializedDSBlock);
  }

  DSBlockSharedPtr blockRetrieved1;
  BlockStorage::GetBlockStorage().GetDSBlock(20, blockRetrieved1);

  LOG_GENERAL(INFO,
              "Block num value entered: " << block.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block num value retrieved: "
                        << (*blockRetrieved1).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block.GetHeader().GetBlockNum() ==
          (*blockRetrieved1).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");

  DSBlockSharedPtr blockRetrieved2;
  BlockStorage::GetBlockStorage().GetDSBlock(0, blockRetrieved2);

  BOOST_CHECK_MESSAGE(
      constructDummyDSBlock(0).GetHeader().GetBlockNum() ==
          (*blockRetrieved2).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");
}

void writeBlock(unsigned int id) {
  DSBlock block = constructDummyDSBlock(id);

  bytes serializedDSBlock;

  block.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(12345 + id, serializedDSBlock);
}

void readBlock(unsigned int id) {
  DSBlockSharedPtr block;
  BlockStorage::GetBlockStorage().GetDSBlock(id, block);

  if ((*block).GetHeader().GetBlockNum() != id) {
    LOG_GENERAL(INFO, "block num is " << (*block).GetHeader().GetBlockNum()
                                      << ", id is " << id);
    if ((*block).GetHeader().GetBlockNum() != id) {
      LOG_GENERAL(FATAL, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
    }
  }
}

void readWriteBlock(int tid) {
  for (int j = 0; j < 100; j++) {
    writeBlock(tid * 100000 + j);
    readBlock(12345 + tid * 1000 + j);
  }
}

void bootstrap(int num_threads) {
  for (int i = 0; i < num_threads; i++) {
    for (int j = 0; j < 100; j++) {
      DSBlock block = constructDummyDSBlock(i * 1000 + j);

      bytes serializedDSBlock;

      block.Serialize(serializedDSBlock, 0);
      BlockStorage::GetBlockStorage().PutDSBlock(12345 + i * 1000 + j,
                                                 serializedDSBlock);
    }
  }

  LOG_GENERAL(INFO, "Bootstrapping done!!");
}

BOOST_AUTO_TEST_CASE(testThreadSafety) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const int num_threads = 20;

  bootstrap(num_threads);

  std::thread t[num_threads];

  // Launch a group of threads
  for (int i = 0; i < num_threads; ++i) {
    t[i] = std::thread(readWriteBlock, i);
  }

  std::cout << "Launched from the main\n";

  // Join the threads with the main thread
  for (auto& i : t) {
    i.join();
  }
}

/*
    tests correctness when blocks get written over a series of files
    when running this test change BLOCK_FILE_SIZE to 128*1024*1024/512 in
   BlockStorage.h
*/

BOOST_AUTO_TEST_CASE(testMultipleBlocksInMultipleFiles) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  //     BlockStorage::SetBlockFileSize(128 * ONE_MEGABYTE / 512);
  //     // BlockStorage::m_blockFileSize = 128 * ONE_MEGABYTE / 512;

  DSBlock block = constructDummyDSBlock(0);

  for (int i = 21; i < 250; i++) {
    block = constructDummyDSBlock(i);

    bytes serializedDSBlock;

    block.Serialize(serializedDSBlock, 0);
    BlockStorage::GetBlockStorage().PutDSBlock(i, serializedDSBlock);
  }

  DSBlockSharedPtr blockRetrieved;
  BlockStorage::GetBlockStorage().GetDSBlock(249, blockRetrieved);

  LOG_GENERAL(INFO,
              "Block num value entered: " << block.GetHeader().GetBlockNum());
  LOG_GENERAL(INFO, "Block num value retrieved: "
                        << (*blockRetrieved).GetHeader().GetBlockNum());
  BOOST_CHECK_MESSAGE(
      block.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "Block num shouldn't change after writing to/ reading from disk");
}

BOOST_AUTO_TEST_CASE(testRetrieveAllTheDSBlocksInDB) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  if (BlockStorage::GetBlockStorage().ResetDB(BlockStorage::DBTYPE::DS_BLOCK)) {
    std::list<DSBlock> in_blocks;

    for (int i = 0; i < 10; i++) {
      DSBlock block = constructDummyDSBlock(i);

      bytes serializedDSBlock;

      block.Serialize(serializedDSBlock, 0);

      BlockStorage::GetBlockStorage().PutDSBlock(i, serializedDSBlock);
      in_blocks.emplace_back(block);
    }

    std::list<DSBlockSharedPtr> ref_blocks;
    std::list<DSBlock> out_blocks;
    BOOST_CHECK_MESSAGE(
        BlockStorage::GetBlockStorage().GetAllDSBlocks(ref_blocks),
        "GetAllDSBlocks shouldn't fail");
    for (const auto& i : ref_blocks) {
      out_blocks.emplace_back(*i);
    }
    BOOST_CHECK_MESSAGE(
        in_blocks == out_blocks,
        "DSBlocks shouldn't change after writting to/ reading from disk");
  }
}

BOOST_AUTO_TEST_SUITE_END()
