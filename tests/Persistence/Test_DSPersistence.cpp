/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

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

BOOST_AUTO_TEST_CASE(testReadWriteSimpleStringToDB)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    DB db("test.db");

    db.WriteToDB("fruit", "vegetable");

    std::string ret = db.ReadFromDB("fruit");

    BOOST_CHECK_MESSAGE(
        ret == "vegetable",
        "ERROR: return value from DB not equal to inserted value");
}

DSBlock constructDummyDSBlock(int instanceNum)
{
    BlockHash prevHash1;

    for (unsigned int i = 0; i < prevHash1.asArray().size(); i++)
    {
        prevHash1.asArray().at(i) = i + 1;
    }

    std::pair<PrivKey, PubKey> pubKey1 = Schnorr::GetInstance().GenKeyPair();

    return DSBlock(DSBlockHeader(20, prevHash1, 12345 + instanceNum,
                                 pubKey1.first, pubKey1.second, 10, 789),
                   CoSignatures());
}

BOOST_AUTO_TEST_CASE(testSerializationDeserialization)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    // checking if normal serialization and deserialization of blocks is working or not

    DSBlock block1 = constructDummyDSBlock(0);

    std::vector<unsigned char> serializedDSBlock;
    block1.Serialize(serializedDSBlock, 0);

    DSBlock block2(serializedDSBlock, 0);

    BOOST_CHECK_MESSAGE(
        block2.GetHeader().GetNonce() == block1.GetHeader().GetNonce(),
        "nonce shouldn't change after serailization and deserialization");
}

BOOST_AUTO_TEST_CASE(testBlockStorage)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    DSBlock block1 = constructDummyDSBlock(0);

    std::vector<unsigned char> serializedDSBlock;
    block1.Serialize(serializedDSBlock, 0);

    BlockStorage::GetBlockStorage().PutDSBlock(0, serializedDSBlock);

    DSBlockSharedPtr block2;
    BlockStorage::GetBlockStorage().GetDSBlock(0, block2);

    // using individual == tests instead of DSBlockHeader::operator== to zero in
    // which particular data type fails on writing to/ reading from disk

    LOG_GENERAL(
        INFO, "Block1 nonce value entered: " << block1.GetHeader().GetNonce());
    LOG_GENERAL(
        INFO,
        "Block2 nonce value retrieved: " << (*block2).GetHeader().GetNonce());
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetNonce() == (*block2).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");

    LOG_GENERAL(INFO,
                "Block1 difficulty value entered: "
                    << (int)(block1.GetHeader().GetDifficulty()));
    LOG_GENERAL(INFO,
                "Block2 difficulty value retrieved: "
                    << (int)((*block2).GetHeader().GetDifficulty()));
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetDifficulty()
            == (*block2).GetHeader().GetDifficulty(),
        "difficulty shouldn't change after writing to/ reading from disk");

    LOG_GENERAL(
        INFO,
        "Block1 blocknum value entered: " << block1.GetHeader().GetBlockNum());
    LOG_GENERAL(INFO,
                "Block2 blocknum value retrieved: "
                    << (*block2).GetHeader().GetBlockNum());
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetBlockNum() == (*block2).GetHeader().GetBlockNum(),
        "blocknum shouldn't change after writing to/ reading from disk");

    LOG_GENERAL(INFO,
                "Block1 timestamp value entered: "
                    << block1.GetHeader().GetTimestamp());
    LOG_GENERAL(INFO,
                "Block2 timestamp value retrieved: "
                    << (*block2).GetHeader().GetTimestamp());
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetTimestamp()
            == (*block2).GetHeader().GetTimestamp(),
        "timestamp shouldn't change after writing to/ reading from disk");

    // LOG_GENERAL(INFO, "Block1 MinerPubKey value entered: " << block1.GetHeader().GetMinerPubKey());
    // LOG_GENERAL(INFO, "Block2 MinerPubKey value retrieved: " << (*block2).GetHeader().GetMinerPubKey());
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetMinerPubKey()
            == (*block2).GetHeader().GetMinerPubKey(),
        "MinerPubKey shouldn't change after writing to/ reading from disk");

    // LOG_GENERAL(INFO, "Block1 LeaderPubKey value entered: " << block1.GetHeader().GetLeaderPubKey());
    // LOG_GENERAL(INFO, "Block2 LeaderPubKey value retrieved: " << (*block2).GetHeader().GetLeaderPubKey());
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetLeaderPubKey()
            == (*block2).GetHeader().GetLeaderPubKey(),
        "LeaderPubKey shouldn't change after writing to/ reading from disk");

    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetPrevHash() == (*block2).GetHeader().GetPrevHash(),
        "PrevHash shouldn't change after writing to/ reading from disk");

    BOOST_CHECK_MESSAGE(
        block1.GetCS2() == (*block2).GetCS2(),
        "Signature shouldn't change after writing to/ reading from disk");
}

BOOST_AUTO_TEST_CASE(testRandomBlockAccesses)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    DSBlock block1 = constructDummyDSBlock(1);
    DSBlock block2 = constructDummyDSBlock(2);
    DSBlock block3 = constructDummyDSBlock(3);
    DSBlock block4 = constructDummyDSBlock(4);

    std::vector<unsigned char> serializedDSBlock;

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
                "Block nonce value entered: " << block2.GetHeader().GetNonce());
    LOG_GENERAL(INFO,
                "Block nonce value retrieved: "
                    << (*blockRetrieved).GetHeader().GetNonce());
    BOOST_CHECK_MESSAGE(
        block2.GetHeader().GetNonce()
            == (*blockRetrieved).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");

    BlockStorage::GetBlockStorage().GetDSBlock(4, blockRetrieved);

    LOG_GENERAL(INFO,
                "Block nonce value entered: " << block4.GetHeader().GetNonce());
    LOG_GENERAL(INFO,
                "Block nonce value retrieved: "
                    << (*blockRetrieved).GetHeader().GetNonce());
    BOOST_CHECK_MESSAGE(
        block4.GetHeader().GetNonce()
            == (*blockRetrieved).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");

    BlockStorage::GetBlockStorage().GetDSBlock(1, blockRetrieved);

    LOG_GENERAL(INFO,
                "Block nonce value entered: " << block1.GetHeader().GetNonce());
    LOG_GENERAL(INFO,
                "Block nonce value retrieved: "
                    << (*blockRetrieved).GetHeader().GetNonce());
    BOOST_CHECK_MESSAGE(
        block1.GetHeader().GetNonce()
            == (*blockRetrieved).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");
}

BOOST_AUTO_TEST_CASE(testCachedAndEvictedBlocks)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    DSBlock block = constructDummyDSBlock(0);

    for (int i = 5; i < 21; i++)
    {
        block = constructDummyDSBlock(i);

        std::vector<unsigned char> serializedDSBlock;

        block.Serialize(serializedDSBlock, 0);
        BlockStorage::GetBlockStorage().PutDSBlock(i, serializedDSBlock);
    }

    DSBlockSharedPtr blockRetrieved1;
    BlockStorage::GetBlockStorage().GetDSBlock(20, blockRetrieved1);

    LOG_GENERAL(INFO,
                "Block nonce value entered: " << block.GetHeader().GetNonce());
    LOG_GENERAL(INFO,
                "Block nonce value retrieved: "
                    << (*blockRetrieved1).GetHeader().GetNonce());
    BOOST_CHECK_MESSAGE(
        block.GetHeader().GetNonce()
            == (*blockRetrieved1).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");

    DSBlockSharedPtr blockRetrieved2;
    BlockStorage::GetBlockStorage().GetDSBlock(0, blockRetrieved2);

    BOOST_CHECK_MESSAGE(
        constructDummyDSBlock(0).GetHeader().GetNonce()
            == (*blockRetrieved2).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");
}

void writeBlock(int id)
{
    DSBlock block = constructDummyDSBlock(id);

    std::vector<unsigned char> serializedDSBlock;

    block.Serialize(serializedDSBlock, 0);
    BlockStorage::GetBlockStorage().PutDSBlock(12345 + id, serializedDSBlock);
}

void readBlock(int id)
{
    DSBlockSharedPtr block;
    BlockStorage::GetBlockStorage().GetDSBlock(id, block);
    if ((*block).GetHeader().GetNonce() != id)
    {
        LOG_GENERAL(INFO,
                    "nonce is " << (*block).GetHeader().GetNonce() << ", id is "
                                << id);

        if ((*block).GetHeader().GetNonce() != id)
        {
            LOG_GENERAL(FATAL,
                        "assertion failed (" << __FILE__ << ":" << __LINE__
                                             << ": " << __FUNCTION__ << ")");
        }
    }
}

void readWriteBlock(int tid)
{
    for (int j = 0; j < 100; j++)
    {
        writeBlock(tid * 100000 + j);
        readBlock(12345 + tid * 1000 + j);
    }
}

void bootstrap(int num_threads)
{
    for (int i = 0; i < num_threads; i++)
    {
        for (int j = 0; j < 100; j++)
        {
            DSBlock block = constructDummyDSBlock(i * 1000 + j);

            std::vector<unsigned char> serializedDSBlock;

            block.Serialize(serializedDSBlock, 0);
            BlockStorage::GetBlockStorage().PutDSBlock(12345 + i * 1000 + j,
                                                       serializedDSBlock);
        }
    }

    LOG_GENERAL(INFO, "Bootstrapping done!!");
}

BOOST_AUTO_TEST_CASE(testThreadSafety)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    const int num_threads = 20;

    bootstrap(num_threads);

    std::thread t[num_threads];

    //Launch a group of threads
    for (int i = 0; i < num_threads; ++i)
    {
        t[i] = std::thread(readWriteBlock, i);
    }

    std::cout << "Launched from the main\n";

    //Join the threads with the main thread
    for (int i = 0; i < num_threads; ++i)
    {
        t[i].join();
    }
}

/*
    tests correctness when blocks get written over a series of files
    when running this test change BLOCK_FILE_SIZE to 128*1024*1024/512 in BlockStorage.h
*/
BOOST_AUTO_TEST_CASE(testMultipleBlocksInMultipleFiles)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    //     BlockStorage::SetBlockFileSize(128 * ONE_MEGABYTE / 512);
    //     // BlockStorage::m_blockFileSize = 128 * ONE_MEGABYTE / 512;

    DSBlock block = constructDummyDSBlock(0);

    for (int i = 21; i < 2500; i++)
    {
        block = constructDummyDSBlock(i);

        std::vector<unsigned char> serializedDSBlock;

        block.Serialize(serializedDSBlock, 0);
        BlockStorage::GetBlockStorage().PutDSBlock(i, serializedDSBlock);
    }

    DSBlockSharedPtr blockRetrieved;
    BlockStorage::GetBlockStorage().GetDSBlock(2499, blockRetrieved);

    LOG_GENERAL(INFO,
                "Block nonce value entered: " << block.GetHeader().GetNonce());
    LOG_GENERAL(INFO,
                "Block nonce value retrieved: "
                    << (*blockRetrieved).GetHeader().GetNonce());
    BOOST_CHECK_MESSAGE(
        block.GetHeader().GetNonce()
            == (*blockRetrieved).GetHeader().GetNonce(),
        "nonce shouldn't change after writing to/ reading from disk");

    //     // BlockStorage::m_blockFileSize = 128 * 1024 * 1024;
    //     BlockStorage::SetBlockFileSize(128 * ONE_MEGABYTE);
}

BOOST_AUTO_TEST_CASE(testRetrieveAllTheDSBlocksInDB)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    if (BlockStorage::GetBlockStorage().ResetDB(BlockStorage::DBTYPE::DS_BLOCK))
    {
        std::list<DSBlock> in_blocks;

        for (int i = 0; i < 10; i++)
        {
            DSBlock block = constructDummyDSBlock(i);

            std::vector<unsigned char> serializedDSBlock;

            block.Serialize(serializedDSBlock, 0);

            BlockStorage::GetBlockStorage().PutDSBlock(i, serializedDSBlock);
            in_blocks.push_back(block);
        }

        std::list<DSBlockSharedPtr> ref_blocks;
        std::list<DSBlock> out_blocks;
        BOOST_CHECK_MESSAGE(
            BlockStorage::GetBlockStorage().GetAllDSBlocks(ref_blocks),
            "GetAllDSBlocks shouldn't fail");
        for (auto i : ref_blocks)
        {
            LOG_GENERAL(INFO, i->GetHeader().GetNonce());
            out_blocks.push_back(*i);
        }
        BOOST_CHECK_MESSAGE(
            in_blocks == out_blocks,
            "DSBlocks shouldn't change after writting to/ reading from disk");
    }
}

BOOST_AUTO_TEST_SUITE_END()
