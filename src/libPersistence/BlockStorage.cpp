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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <leveldb/db.h>

#include "BlockStorage.h"
#include "common/Constants.h"
#include "common/Serializable.h"

using namespace std;

BlockStorage& BlockStorage::GetBlockStorage()
{
    static BlockStorage bs;
    return bs;
}

#ifndef IS_LOOKUP_NODE
bool BlockStorage::PushBackTxBodyDB(
    const boost::multiprecision::uint256_t& blockNum)
{
    LOG_MARKER();

    if (m_txBodyDBs.size()
        >= NUM_DS_KEEP_TX_BODY + 1) // Leave one for keeping tmp txBody
    {
        LOG_MESSAGE("TxBodyDB pool is full")
        return false;
    }

    LevelDB txBodyDB(blockNum.convert_to<string>(), TX_BODY_SUBDIR);
    m_txBodyDBs.push_back(txBodyDB);

    return true;
}

bool BlockStorage::PopFrontTxBodyDB(bool mandatory)
{
    LOG_MARKER();

    if (!m_txBodyDBs.size())
    {
        LOG_MESSAGE("No TxBodyDB found");
        return false;
    }

    if (!mandatory)
    {
        if (m_txBodyDBs.size() <= NUM_DS_KEEP_TX_BODY)
        {
            LOG_MESSAGE("size of txBodyDB hasn't meet maximum, ignore");
            return true;
        }
    }

    int ret = -1;
    ret = m_txBodyDBs.front().DeleteDB();
    m_txBodyDBs.pop_front();

    return (ret == 0);
}

unsigned int BlockStorage::GetTxBodyDBSize() { return m_txBodyDBs.size(); }
#endif // IS_LOOKUP_NODE

bool BlockStorage::PutBlock(const boost::multiprecision::uint256_t& blockNum,
                            const vector<unsigned char>& body,
                            const BlockType& blockType)
{
    int ret = -1; // according to LevelDB::Insert return vale
    if (blockType == BlockType::DS)
    {
        ret = m_dsBlockchainDB.Insert(blockNum, body);
    }
    else if (blockType == BlockType::Tx)
    {
        ret = m_txBlockchainDB.Insert(blockNum, body);
    }
    return (ret == 0);
}

bool BlockStorage::PutDSBlock(const boost::multiprecision::uint256_t& blockNum,
                              const vector<unsigned char>& body)
{
    bool ret = false;
    if (PutBlock(blockNum, body, BlockType::DS))
    {
        if (PutMetadata(MetaType::DSINCOMPLETED, {'1'}))
        {
            ret = true;
        }
        else
        {
            if (!DeleteDSBlock(blockNum))
            {
                LOG_MESSAGE("FAIL: Delete DSBlock" << blockNum << "Failed");
            }
        }
    }
    return ret;
}

bool BlockStorage::PutTxBlock(const boost::multiprecision::uint256_t& blockNum,
                              const vector<unsigned char>& body)
{
    return PutBlock(blockNum, body, BlockType::Tx);
}

bool BlockStorage::PutTxBody(const dev::h256& key,
                             const vector<unsigned char>& body)
{
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    if (!m_txBodyDBs.size())
    {
        LOG_MESSAGE("Error: No TxBodyDB found");
        return false;
    }
    int ret = m_txBodyDBs.back().Insert(key, body);
#else // IS_LOOKUP_NODE
    int ret = m_txBodyDB.Insert(key, body) && m_txBodyTmpDB.Insert(key, body);
#endif // IS_LOOKUP_NODE

    return (ret == 0);
}

bool BlockStorage::GetDSBlock(const boost::multiprecision::uint256_t& blockNum,
                              DSBlockSharedPtr& block)
{
    string blockString = m_dsBlockchainDB.Lookup(blockNum);

    if (blockString.empty())
    {
        return false;
    }

    LOG_MESSAGE(blockString);
    LOG_MESSAGE(blockString.length());
    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(blockString.c_str());
    // FIXME: Handle exceptions
    block = DSBlockSharedPtr(new DSBlock(
        std::vector<unsigned char>(raw_memory, raw_memory + blockString.size()),
        0));
    return true;
}

bool BlockStorage::GetTxBlock(const boost::multiprecision::uint256_t& blockNum,
                              TxBlockSharedPtr& block)
{
    string blockString = m_txBlockchainDB.Lookup(blockNum);

    if (blockString.empty())
    {
        return false;
    }

    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(blockString.c_str());
    // FIXME: Handle exceptions
    block = TxBlockSharedPtr(new TxBlock(
        std::vector<unsigned char>(raw_memory, raw_memory + blockString.size()),
        0));
    return true;
}

bool BlockStorage::GetTxBody(const dev::h256& key, TxBodySharedPtr& body)
{
#ifndef IS_LOOKUP_NODE
    if (!m_txBodyDBs.size())
    {
        LOG_MESSAGE("Error: No TxBodyDB found");
        return false;
    }
    string bodyString = m_txBodyDBs.back().Lookup(key);
#else // IS_LOOKUP_NODE
    string bodyString = m_txBodyDB.Lookup(key);
#endif

    if (bodyString.empty())
    {
        return false;
    }

    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(bodyString.c_str());
    // FIXME: Handle exceptions
    body = TxBodySharedPtr(new Transaction(
        std::vector<unsigned char>(raw_memory, raw_memory + bodyString.size()),
        0));
    return true;
}

bool BlockStorage::DeleteDSBlock(
    const boost::multiprecision::uint256_t& blocknum)
{
    LOG_MESSAGE("Delete DSBlock Num: " << blocknum);
    int ret = m_dsBlockchainDB.DeleteKey(blocknum);
    return (ret == 0);
}

bool BlockStorage::DeleteTxBlock(
    const boost::multiprecision::uint256_t& blocknum)
{
    int ret = m_txBlockchainDB.DeleteKey(blocknum);
    return (ret == 0);
}

bool BlockStorage::DeleteTxBody(const dev::h256& key)
{
#ifndef IS_LOOKUP_NODE
    int ret = m_txBodyDBs.back().DeleteKey(key);
#else // IS_LOOKUP_NODE
    int ret = m_txBodyTmpDB.DeleteKey(key);
#endif // IS_LOOKUP_NODE

    return (ret == 0);
}

// bool BlockStorage::PutTxBody(const string & key, const vector<unsigned char> & body)
// {
//     int ret = m_txBodyDB.Insert(key, body);
//     return (ret == 0);
// }

// void BlockStorage::GetTxBody(const string & key, TxBodySharedPtr & body)
// {
//     string bodyString = m_txBodyDB.Lookup(key);
//     const unsigned char* raw_memory = reinterpret_cast<const unsigned char*>(bodyString.c_str());
//     body = TxBodySharedPtr( new Transaction(std::vector<unsigned char>(raw_memory,
//                                             raw_memory + bodyString.size()), 0) );
// }

bool BlockStorage::GetAllDSBlocks(std::list<DSBlockSharedPtr>& blocks)
{
    LOG_MARKER();

    leveldb::Iterator* it
        = m_dsBlockchainDB.GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        string bns = it->key().ToString();
        boost::multiprecision::uint256_t blockNum(bns);
        LOG_MESSAGE("blockNum: " << blockNum);

        string blockString = it->value().ToString();
        if (blockString.empty())
        {
            LOG_MESSAGE("ERROR: Lost one block in the chain");
            return false;
        }
        const unsigned char* raw_memory
            = reinterpret_cast<const unsigned char*>(blockString.c_str());
        DSBlockSharedPtr block = DSBlockSharedPtr(
            new DSBlock(std::vector<unsigned char>(
                            raw_memory, raw_memory + blockString.size()),
                        0));

        blocks.push_back(block);
    }

    if (blocks.empty())
    {
        LOG_MESSAGE("Disk has no DSBlock");
        return false;
    }

    return true;
}

bool BlockStorage::GetAllTxBlocks(std::list<TxBlockSharedPtr>& blocks)
{
    LOG_MARKER();

    leveldb::Iterator* it
        = m_txBlockchainDB.GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        string bns = it->key().ToString();
        boost::multiprecision::uint256_t blockNum(bns);
        LOG_MESSAGE("blockNum: " << blockNum);

        string blockString = it->value().ToString();
        if (blockString.empty())
        {
            LOG_MESSAGE("ERROR: Lost one block in the chain");
            return false;
        }
        const unsigned char* raw_memory
            = reinterpret_cast<const unsigned char*>(blockString.c_str());
        TxBlockSharedPtr block = TxBlockSharedPtr(
            new TxBlock(std::vector<unsigned char>(
                            raw_memory, raw_memory + blockString.size()),
                        0));
        blocks.push_back(block);
    }

    if (blocks.empty())
    {
        LOG_MESSAGE("Disk has no TxBlock");
        return false;
    }

    return true;
}

#ifdef IS_LOOKUP_NODE
bool BlockStorage::GetAllTxBodiesTmp(std::list<TxnHash>& txnHashes)
{
    LOG_MARKER();

    leveldb::Iterator* it
        = m_txBodyTmpDB.GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        string hashString = it->key().ToString();
        if (hashString.empty())
        {
            LOG_MESSAGE("ERROR: Lost one Tmp txBody Hash");
            return false;
        }
        TxnHash txnHash(hashString);
        txnHashes.push_back(txnHash);
    }
    return true;
}
#endif // IS_LOOKUP_NODE

bool BlockStorage::PutMetadata(MetaType type,
                               const std::vector<unsigned char>& data)
{
    LOG_MARKER();
    int ret = m_metadataDB.Insert(std::to_string((int)type), data);
    return (ret == 0);
}

bool BlockStorage::GetMetadata(MetaType type, std::vector<unsigned char>& data)
{
    LOG_MARKER();
    string metaString = m_metadataDB.Lookup(std::to_string((int)type));

    if (metaString.empty())
    {
        LOG_MESSAGE("ERROR: Failed to get metadata")
        return false;
    }

    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(metaString.c_str());
    data = std::vector<unsigned char>(raw_memory,
                                      raw_memory + metaString.size());

    return true;
}

bool BlockStorage::ResetDB(DBTYPE type)
{
    bool ret = false;
    switch (type)
    {
    case META:
        ret = m_metadataDB.ResetDB();
    case DS_BLOCK:
        ret = m_dsBlockchainDB.ResetDB();
    case TX_BLOCK:
        ret = m_txBlockchainDB.ResetDB();
#ifndef IS_LOOKUP_NODE
    case TX_BODIES:
    {
        int size_txBodyDBs = m_txBodyDBs.size();
        for (int i = 0; i < size_txBodyDBs; i++)
        {
            if (!PopFrontTxBodyDB(true))
            {
                LOG_MESSAGE("Error: failed to reset TxBodyDB list");
                throw std::exception();
            }
        }
        ret = true;
    }
#else // IS_LOOKUP_NODE
    case TX_BODY:
        ret = m_txBodyDB.ResetDB();
    case TX_BODY_TMP:
        ret = m_txBodyTmpDB.ResetDB();
#endif // IS_LOOKUP_NODE
    }
    if (!ret)
    {
        LOG_MESSAGE("FAIL: Reset DB " << type << " failed");
    }
    return ret;
}

bool BlockStorage::ResetAll()
{
    return ResetDB(META) && ResetDB(DS_BLOCK) && ResetDB(TX_BLOCK)
#ifndef IS_LOOKUP_NODE
        && ResetDB(TX_BODIES);
#else // IS_LOOKUP_NODE
        && ResetDB(TX_BODY) && ResetDB(TX_BODY_TMP);
#endif
}