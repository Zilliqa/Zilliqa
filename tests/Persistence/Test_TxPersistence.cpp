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

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testReadWriteSimpleStringToDB) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DB db("test.db");

  db.WriteToDB("fruit", "vegetable");

  string ret = db.ReadFromDB("fruit");

  BOOST_CHECK_MESSAGE(
      ret == "vegetable",
      "ERROR: return value from DB not equal to inserted value");
}

TxBlock constructDummyTxBlock(int instanceNum) {
  // array<unsigned char, BLOCK_HASH_SIZE> emptyHash = { 0 };

  PairOfKey pubKey1 = Schnorr::GenKeyPair();

  return TxBlock(
      TxBlockHeader(1, 1, 1, instanceNum, TxBlockHashSet(), 5, pubKey1.second,
                    instanceNum, TXBLOCK_VERSION, CommitteeHash(), BlockHash()),
      vector<MicroBlockInfo>(1), CoSignatures());
}

void purgeTxBlockData() {
  {
    LevelDB txBlockchainDB{"txBlocks"};
    LevelDB txBlockchainHashToNum{"txBlockHashToNum"};
    LevelDB txBlockchainAux{"txBlocksAux"};

    txBlockchainDB.DeleteDBForNormalNode();
    txBlockchainHashToNum.DeleteDBForNormalNode();
    txBlockchainAux.DeleteDBForNormalNode();
  }
}

BOOST_AUTO_TEST_CASE(testSerializationDeserialization) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // checking if normal serialization and deserialization of blocks is working
  // or not

  TxBlock block1 = constructDummyTxBlock(0);

  bytes serializedTxBlock;
  block1.Serialize(serializedTxBlock, 0);

  TxBlock block2(serializedTxBlock, 0);

  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetBlockNum() == block2.GetHeader().GetBlockNum(),
      "nonce shouldn't change after serailization and deserialization");
}

BOOST_AUTO_TEST_CASE(testBlockStorage) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TxBlock block1 = constructDummyTxBlock(0);

  bytes serializedTxBlock;
  block1.Serialize(serializedTxBlock, 0);

  BlockStorage::GetBlockStorage().PutTxBlock(block1.GetHeader(),
                                             serializedTxBlock);

  TxBlockSharedPtr block2;
  BlockStorage::GetBlockStorage().GetTxBlock(0, block2);

  BOOST_CHECK_MESSAGE(
      block1 == *block2,
      "block shouldn't change after writing to/ reading from disk");
}

BOOST_AUTO_TEST_CASE(testRandomBlockAccesses) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TxBlock block1 = constructDummyTxBlock(1);
  TxBlock block2 = constructDummyTxBlock(2);
  TxBlock block3 = constructDummyTxBlock(3);
  TxBlock block4 = constructDummyTxBlock(4);

  bytes serializedTxBlock;

  block1.Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(block1.GetHeader(),
                                             serializedTxBlock);

  serializedTxBlock.clear();
  block2.Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(block2.GetHeader(),
                                             serializedTxBlock);

  serializedTxBlock.clear();
  block3.Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(block3.GetHeader(),
                                             serializedTxBlock);

  serializedTxBlock.clear();
  block4.Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(block4.GetHeader(),
                                             serializedTxBlock);

  TxBlockSharedPtr blockRetrieved;
  BlockStorage::GetBlockStorage().GetTxBlock(2, blockRetrieved);

  BOOST_CHECK_MESSAGE(
      block2.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");

  BlockStorage::GetBlockStorage().GetTxBlock(4, blockRetrieved);

  BOOST_CHECK_MESSAGE(
      block4.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");

  BlockStorage::GetBlockStorage().GetTxBlock(1, blockRetrieved);

  BOOST_CHECK_MESSAGE(
      block1.GetHeader().GetBlockNum() ==
          (*blockRetrieved).GetHeader().GetBlockNum(),
      "block num shouldn't change after writing to/ reading from disk");
}

BOOST_AUTO_TEST_CASE(testCachedAndEvictedBlocks) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TxBlock block = constructDummyTxBlock(0);

  for (int i = 5; i < 21; i++) {
    block = constructDummyTxBlock(i);

    bytes serializedTxBlock;

    block.Serialize(serializedTxBlock, 0);
    BlockStorage::GetBlockStorage().PutTxBlock(block.GetHeader(),
                                               serializedTxBlock);
  }

  TxBlockSharedPtr blockRetrieved1;
  BlockStorage::GetBlockStorage().GetTxBlock(20, blockRetrieved1);

  BOOST_CHECK_MESSAGE(
      block.GetHeader().GetDSBlockNum() ==
          (*blockRetrieved1).GetHeader().GetDSBlockNum(),
      "block number shouldn't change after writing to/ reading from disk");

  TxBlockSharedPtr blockRetrieved2;
  BlockStorage::GetBlockStorage().GetTxBlock(0, blockRetrieved2);

  BOOST_CHECK_MESSAGE(
      constructDummyTxBlock(0).GetHeader().GetDSBlockNum() ==
          (*blockRetrieved2).GetHeader().GetDSBlockNum(),
      "block number shouldn't change after writing to/ reading from disk");
}

void writeBlock(int id) {
  TxBlock block = constructDummyTxBlock(id);

  bytes serializedDSBlock;

  block.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(block.GetHeader(),
                                             serializedDSBlock);
}

void readBlock(int id) {
  TxBlockSharedPtr block;
  BlockStorage::GetBlockStorage().GetTxBlock(id, block);
  if ((*block).GetHeader().GetBlockNum() != (uint64_t)id) {
    LOG_GENERAL(INFO, "GetBlockNum is " << (*block).GetHeader().GetBlockNum()
                                        << ", id is " << id);

    if ((*block).GetHeader().GetBlockNum() != (uint64_t)id) {
      LOG_GENERAL(FATAL, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
    }
  } else {
    LOG_GENERAL(INFO, "GetBlockNum is " << (*block).GetHeader().GetBlockNum()
                                        << ", id is " << id);
  }
}

void readWriteBlock(int tid) {
  for (int j = 0; j < 100; j++) {
    writeBlock(tid * 100000 + j);
    readBlock(tid * 1000 + j);
  }
}

void bootstrap(int num_threads) {
  for (int i = 0; i < num_threads; i++) {
    for (int j = 0; j < 100; j++) {
      TxBlock block = constructDummyTxBlock(i * 1000 + j);

      bytes serializedTxBlock;

      block.Serialize(serializedTxBlock, 0);
      BlockStorage::GetBlockStorage().PutTxBlock(block.GetHeader(),
                                                 serializedTxBlock);
    }
  }

  LOG_GENERAL(INFO, "Bootstrapping done!!");
}

BOOST_AUTO_TEST_CASE(testThreadSafety) {
  INIT_STDOUT_LOGGER();

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
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // BlockStorage::SetBlockFileSize(128 * ONE_MEGABYTE / 512);
  // BlockStorage::m_blockFileSize = 128 * ONE_MEGABYTE / 512;

  TxBlock block = constructDummyTxBlock(0);

  for (int i = 21; i < 2500; i++) {
    block = constructDummyTxBlock(i);

    bytes serializedTxBlock;

    block.Serialize(serializedTxBlock, 0);
    BlockStorage::GetBlockStorage().PutTxBlock(block.GetHeader(),
                                               serializedTxBlock);
  }

  TxBlockSharedPtr blockRetrieved;
  BlockStorage::GetBlockStorage().GetTxBlock(2499, blockRetrieved);

  BOOST_CHECK_MESSAGE(
      block.GetHeader().GetDSBlockNum() ==
          (*blockRetrieved).GetHeader().GetDSBlockNum(),
      "block number shouldn't change after writing to/ reading from disk");

  // BlockStorage::m_blockFileSize = 128 * ONE_MEGABYTE;
  // BlockStorage::SetBlockFileSize(128 * ONE_MEGABYTE);
}

BOOST_AUTO_TEST_CASE(testRetrieveAllTheTxBlocksInDB) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  if (BlockStorage::GetBlockStorage().ResetDB(BlockStorage::DBTYPE::TX_BLOCK)) {
    std::list<TxBlock> in_blocks;

    for (int i = 0; i < 10; i++) {
      TxBlock block = constructDummyTxBlock(i);

      bytes serializedTxBlock;

      block.Serialize(serializedTxBlock, 0);

      BlockStorage::GetBlockStorage().PutTxBlock(block.GetHeader(),
                                                 serializedTxBlock);
      in_blocks.emplace_back(block);
    }

    std::deque<TxBlockSharedPtr> ref_blocks;
    std::list<TxBlock> out_blocks;
    BOOST_CHECK_MESSAGE(
        BlockStorage::GetBlockStorage().GetAllTxBlocks(ref_blocks),
        "GetAllDSBlocks shouldn't fail");
    for (const auto& i : ref_blocks) {
      LOG_GENERAL(INFO, i->GetHeader().GetDSBlockNum());
      out_blocks.emplace_back(*i);
    }
    BOOST_CHECK_MESSAGE(
        (in_blocks.size() == out_blocks.size()) &&
            equal(in_blocks.begin(), in_blocks.end(), out_blocks.begin()),
        "DSBlocks shouldn't change after writting to/ reading from disk");
  }
}

BOOST_AUTO_TEST_CASE(testBuildPendingTxHashToNumMapping) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // Release held lock
  BlockStorage::GetBlockStorage().ReleaseDB();

  // Cleanup any db files, release lock afterwards
  purgeTxBlockData();

  constexpr auto INIT_SIZE = 4;
  constexpr auto REMAINING_SIZE = 8;
  std::vector<TxBlock> alreadyKnownBlocks;

  {
    LevelDB txBlockchainDB{"txBlocks"};
    LevelDB txBlockchainHashToNum{"txBlockHashToNum"};
    LevelDB txBlockchainAux{"txBlocksAux"};

    // Build some initial hash to block mapping
    for (uint32_t i = 0; i < INIT_SIZE; i++) {
      const TxBlock block = constructDummyTxBlock(i);
      bytes serializedTxBlock;
      block.Serialize(serializedTxBlock, 0);

      const auto blockNum = block.GetHeader().GetBlockNum();
      txBlockchainDB.Insert(blockNum, serializedTxBlock);
      txBlockchainHashToNum.Insert(block.GetBlockHash(),
                                   std::to_string(blockNum));
      alreadyKnownBlocks.emplace_back(block);

      txBlockchainAux.Insert(leveldb::Slice{MAX_TX_BLOCK_NUM_KEY},
                             leveldb::Slice{std::to_string(blockNum)});
    }

    // Store remaining blocks only by 'blockNum'
    for (uint32_t i = INIT_SIZE; i < REMAINING_SIZE; i++) {
      TxBlock block = constructDummyTxBlock(i);
      bytes serializedTxBlock;
      block.Serialize(serializedTxBlock, 0);

      txBlockchainDB.Insert(block.GetHeader().GetBlockNum(), serializedTxBlock);
      alreadyKnownBlocks.emplace_back(block);
    }
  }

  BlockStorage::GetBlockStorage().Initialize();
  // This should trigger proper rebuilt of the missing hash->num mapping
  BlockStorage::GetBlockStorage().RefreshAll();

  // All blocks should be now queried by hash/num
  for (uint32_t i = 0; i < alreadyKnownBlocks.size(); ++i) {
    const auto& iteratedBlock = alreadyKnownBlocks[i];
    TxBlockSharedPtr blockRetrieved;

    // Query by num
    BlockStorage::GetBlockStorage().GetTxBlock(
        iteratedBlock.GetHeader().GetBlockNum(), blockRetrieved);
    BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
                iteratedBlock.GetHeader().GetBlockNum());
    BOOST_CHECK(blockRetrieved->GetBlockHash() == iteratedBlock.GetBlockHash());

    blockRetrieved.reset();

    // Query by hash
    BlockStorage::GetBlockStorage().GetTxBlock(iteratedBlock.GetBlockHash(),
                                               blockRetrieved);
    BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
                iteratedBlock.GetHeader().GetBlockNum());
    BOOST_CHECK(blockRetrieved->GetBlockHash() == iteratedBlock.GetBlockHash());
  }
}

BOOST_AUTO_TEST_CASE(testSkipBuildMappingEmptyAuxData) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // Release held lock
  BlockStorage::GetBlockStorage().ReleaseDB();

  // Cleanup any db files, release lock afterwards
  purgeTxBlockData();

  std::vector<TxBlock> alreadyKnownBlocks;
  TxBlock block = constructDummyTxBlock(0);

  {
    LevelDB txBlockchainDB{"txBlocks"};
    const auto blockNum = block.GetHeader().GetBlockNum();
    bytes serializedTxBlock;
    block.Serialize(serializedTxBlock, 0);
    txBlockchainDB.Insert(blockNum, serializedTxBlock);
  }

  BlockStorage::GetBlockStorage().Initialize();
  // This should not trigger proper rebuilt of the missing hash->num mapping
  // (due to missing Aux info)
  BlockStorage::GetBlockStorage().RefreshAll();

  TxBlockSharedPtr blockRetrieved;

  // Query by num (this is ok)
  BlockStorage::GetBlockStorage().GetTxBlock(block.GetHeader().GetBlockNum(),
                                             blockRetrieved);
  BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
              block.GetHeader().GetBlockNum());
  BOOST_CHECK(blockRetrieved->GetBlockHash() == block.GetBlockHash());

  blockRetrieved.reset();

  // Query by hash (should be empty)
  BlockStorage::GetBlockStorage().GetTxBlock(block.GetBlockHash(),
                                             blockRetrieved);
  BOOST_CHECK(blockRetrieved.get() == nullptr);
}

BOOST_AUTO_TEST_CASE(testNoNeedToBuildTxHashToNumMapping) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // Release held lock
  BlockStorage::GetBlockStorage().ReleaseDB();

  // Cleanup any db files, release lock afterwards
  purgeTxBlockData();

  constexpr auto NUM_BLOCKS = 4;
  std::vector<TxBlock> alreadyKnownBlocks;

  {
    LevelDB txBlockchainDB{"txBlocks"};
    LevelDB txBlockchainHashToNum{"txBlockHashToNum"};
    LevelDB txBlockchainAux{"txBlocksAux"};

    // Build some initial hash to block mapping
    for (uint32_t i = 0; i < NUM_BLOCKS; i++) {
      TxBlock block = constructDummyTxBlock(i);
      bytes serializedTxBlock;
      block.Serialize(serializedTxBlock, 0);

      const auto blockNum = block.GetHeader().GetBlockNum();
      txBlockchainDB.Insert(blockNum, serializedTxBlock);
      txBlockchainHashToNum.Insert(block.GetBlockHash(),
                                   std::to_string(blockNum));
      alreadyKnownBlocks.emplace_back(block);

      txBlockchainAux.Insert(leveldb::Slice{MAX_TX_BLOCK_NUM_KEY},
                             leveldb::Slice{std::to_string(blockNum)});
    }
  }

  BlockStorage::GetBlockStorage().Initialize();
  // This should trigger proper rebuilt of the missing hash->num mapping (if
  // needed)
  BlockStorage::GetBlockStorage().RefreshAll();

  // All blocks should be queried by hash/num
  for (uint32_t i = 0; i < alreadyKnownBlocks.size(); ++i) {
    const auto& iteratedBlock = alreadyKnownBlocks[i];
    TxBlockSharedPtr blockRetrieved;

    // Query by num
    BlockStorage::GetBlockStorage().GetTxBlock(
        iteratedBlock.GetHeader().GetBlockNum(), blockRetrieved);
    BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
                iteratedBlock.GetHeader().GetBlockNum());
    BOOST_CHECK(blockRetrieved->GetBlockHash() == iteratedBlock.GetBlockHash());

    blockRetrieved.reset();

    // Query by hash
    BlockStorage::GetBlockStorage().GetTxBlock(iteratedBlock.GetBlockHash(),
                                               blockRetrieved);
    BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
                iteratedBlock.GetHeader().GetBlockNum());
    BOOST_CHECK(blockRetrieved->GetBlockHash() == iteratedBlock.GetBlockHash());
  }

  // Out of range blocks should be null for both num/hashq-type queries
  TxBlock block = constructDummyTxBlock(NUM_BLOCKS);
  TxBlockSharedPtr blockRetrieved;

  BlockStorage::GetBlockStorage().GetTxBlock(block.GetHeader().GetBlockNum(),
                                             blockRetrieved);
  BOOST_CHECK(blockRetrieved.get() == nullptr);

  BlockStorage::GetBlockStorage().GetTxBlock(block.GetBlockHash(),
                                             blockRetrieved);
  BOOST_CHECK(blockRetrieved.get() == nullptr);
}

BOOST_AUTO_TEST_CASE(testInsertTxBlockAndQuery) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  constexpr auto BLOCK_NUM = 123;

  TxBlock block = constructDummyTxBlock(BLOCK_NUM);
  bytes serializedTxBlock;
  block.Serialize(serializedTxBlock, 0);

  BlockStorage::GetBlockStorage().PutTxBlock(block.GetHeader(),
                                             serializedTxBlock);
  TxBlockSharedPtr blockRetrieved;

  // Query by num
  BlockStorage::GetBlockStorage().GetTxBlock(BLOCK_NUM, blockRetrieved);
  BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
              block.GetHeader().GetBlockNum());
  BOOST_CHECK(blockRetrieved->GetBlockHash() == block.GetBlockHash());

  blockRetrieved.reset();

  // Query by hash
  BlockStorage::GetBlockStorage().GetTxBlock(block.GetBlockHash(),
                                             blockRetrieved);
  BOOST_CHECK(blockRetrieved->GetHeader().GetBlockNum() ==
              block.GetHeader().GetBlockNum());
  BOOST_CHECK(blockRetrieved->GetBlockHash() == block.GetBlockHash());
}

BOOST_AUTO_TEST_SUITE_END()
