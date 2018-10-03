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

bool BlockStorage::PutBlock(const uint64_t& blockNum,
                            const vector<unsigned char>& body,
                            const BlockType& blockType)
{
    int ret = -1; // according to LevelDB::Insert return value
    if (blockType == BlockType::DS)
    {
        ret = m_dsBlockchainDB->Insert(blockNum, body);
        LOG_GENERAL(INFO, "Stored DsBlock  Num:" << blockNum);
    }
    else if (blockType == BlockType::Tx)
    {
        ret = m_txBlockchainDB->Insert(blockNum, body);
        LOG_GENERAL(INFO, "Stored TxBlock  Num:" << blockNum);
    }
    return (ret == 0);
}

bool BlockStorage::PutDSBlock(const uint64_t& blockNum,
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
                LOG_GENERAL(INFO,
                            "FAIL: Delete DSBlock" << blockNum << "Failed");
            }
        }
    }
    return ret;
}

bool BlockStorage::PutTxBlock(const uint64_t& blockNum,
                              const vector<unsigned char>& body)
{
    return PutBlock(blockNum, body, BlockType::Tx);
}

bool BlockStorage::PutTxBody(const dev::h256& key,
                             const vector<unsigned char>& body)
{
    int ret;

    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING, "Non lookup node should not trigger this.");
        return false;
    }
    else // IS_LOOKUP_NODE
    {
        ret = m_txBodyDB->Insert(key, body) && m_txBodyTmpDB->Insert(key, body);
    }

    return (ret == 0);
}

bool BlockStorage::GetDSBlock(const uint64_t& blockNum, DSBlockSharedPtr& block)
{
    string blockString = m_dsBlockchainDB->Lookup(blockNum);

    if (blockString.empty())
    {
        return false;
    }

    LOG_GENERAL(INFO, blockString);
    LOG_GENERAL(INFO, blockString.length());
    block = DSBlockSharedPtr(new DSBlock(
        std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));

    return true;
}

bool BlockStorage::GetTxBlock(const uint64_t& blockNum, TxBlockSharedPtr& block)
{
    string blockString = m_txBlockchainDB->Lookup(blockNum);

    if (blockString.empty())
    {
        return false;
    }

    block = TxBlockSharedPtr(new TxBlock(
        std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));

    return true;
}

bool BlockStorage::GetTxBody(const dev::h256& key, TxBodySharedPtr& body)
{
    std::string bodyString;
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING, "Non lookup node should not trigger this.");
        return false;
    }
    else // IS_LOOKUP_NODE
    {
        bodyString = m_txBodyDB->Lookup(key);
    }

    if (bodyString.empty())
    {
        return false;
    }
    body = TxBodySharedPtr(new TransactionWithReceipt(
        std::vector<unsigned char>(bodyString.begin(), bodyString.end()), 0));

    return true;
}

bool BlockStorage::DeleteDSBlock(const uint64_t& blocknum)
{
    LOG_GENERAL(INFO, "Delete DSBlock Num: " << blocknum);
    int ret = m_dsBlockchainDB->DeleteKey(blocknum);
    return (ret == 0);
}

bool BlockStorage::DeleteTxBlock(const uint64_t& blocknum)
{
    LOG_GENERAL(INFO, "Delete TxBlock Num: " << blocknum);
    int ret = m_txBlockchainDB->DeleteKey(blocknum);
    return (ret == 0);
}

bool BlockStorage::DeleteTxBody(const dev::h256& key)
{
    int ret;
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING, "Non lookup node should not trigger this");
        return false;
    }
    else
    {
        ret = m_txBodyDB->DeleteKey(key);
    }

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
        = m_dsBlockchainDB->GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        string bns = it->key().ToString();
        string blockString = it->value().ToString();
        if (blockString.empty())
        {
            LOG_GENERAL(WARNING, "Lost one block in the chain");
            delete it;
            return false;
        }

        DSBlockSharedPtr block = DSBlockSharedPtr(new DSBlock(
            std::vector<unsigned char>(blockString.begin(), blockString.end()),
            0));
        blocks.emplace_back(block);
        LOG_GENERAL(INFO, "Retrievd DsBlock Num:" << bns);
    }

    delete it;

    if (blocks.empty())
    {
        LOG_GENERAL(INFO, "Disk has no DSBlock");
        return false;
    }

    return true;
}

bool BlockStorage::GetAllTxBlocks(std::list<TxBlockSharedPtr>& blocks)
{
    LOG_MARKER();

    leveldb::Iterator* it
        = m_txBlockchainDB->GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        string bns = it->key().ToString();
        string blockString = it->value().ToString();
        if (blockString.empty())
        {
            LOG_GENERAL(WARNING, "Lost one block in the chain");
            delete it;
            return false;
        }
        TxBlockSharedPtr block = TxBlockSharedPtr(new TxBlock(
            std::vector<unsigned char>(blockString.begin(), blockString.end()),
            0));
        blocks.emplace_back(block);
        LOG_GENERAL(INFO, "Retrievd TxBlock Num:" << bns);
    }

    delete it;

    if (blocks.empty())
    {
        LOG_GENERAL(INFO, "Disk has no TxBlock");
        return false;
    }

    return true;
}

bool BlockStorage::GetAllTxBodiesTmp(std::list<TxnHash>& txnHashes)
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "BlockStorage::GetAllTxBodiesTmp not expected to be called "
                    "from other than the LookUp node.");
        return true;
    }

    LOG_MARKER();

    leveldb::Iterator* it
        = m_txBodyTmpDB->GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        string hashString = it->key().ToString();
        if (hashString.empty())
        {
            LOG_GENERAL(WARNING, "Lost one Tmp txBody Hash");
            delete it;
            return false;
        }
        TxnHash txnHash(hashString);
        txnHashes.emplace_back(txnHash);
    }

    delete it;
    return true;
}

bool BlockStorage::PutMetadata(MetaType type,
                               const std::vector<unsigned char>& data)
{
    LOG_MARKER();
    int ret = m_metadataDB->Insert(std::to_string((int)type), data);
    return (ret == 0);
}

bool BlockStorage::GetMetadata(MetaType type, std::vector<unsigned char>& data)
{
    LOG_MARKER();
    string metaString = m_metadataDB->Lookup(std::to_string((int)type));

    if (metaString.empty())
    {
        LOG_GENERAL(INFO, "No metadata get")
        return false;
    }

    data = std::vector<unsigned char>(metaString.begin(), metaString.end());

    return true;
}

bool BlockStorage::PutDSCommittee(
    const shared_ptr<deque<pair<PubKey, Peer>>>& dsCommittee,
    const uint16_t& consensusLeaderID)
{
    LOG_MARKER();

    unsigned int index = 0;
    string leaderId = to_string(consensusLeaderID);

    if (0
        != m_dsCommitteeDB->Insert(
               index++,
               vector<unsigned char>(leaderId.begin(), leaderId.end())))
    {
        LOG_GENERAL(WARNING,
                    "Failed to store DS leader ID:" << consensusLeaderID);
        return false;
    }

    LOG_GENERAL(INFO, "Stored DS leader ID:" << consensusLeaderID);

    vector<unsigned char> data;

    for (const auto& ds : *dsCommittee)
    {
        int pubKeySize = ds.first.Serialize(data, 0);
        ds.second.Serialize(data, pubKeySize);

        /// Store index as key, to guarantee the sequence of DS committee after retrieval
        /// Because first DS committee is DS leader
        if (0 != m_dsCommitteeDB->Insert(index++, data))
        {
            LOG_GENERAL(WARNING,
                        "Failed to store DS committee:" << ds.first << ", "
                                                        << ds.second);
            return false;
        }

        LOG_GENERAL(INFO,
                    "Stored DS committee:" << ds.first << ", " << ds.second);
    }

    return true;
}

bool BlockStorage::GetDSCommittee(
    shared_ptr<deque<pair<PubKey, Peer>>>& dsCommittee,
    uint16_t& consensusLeaderID)
{
    LOG_MARKER();

    unsigned int index = 0;
    consensusLeaderID = stoul(m_dsCommitteeDB->Lookup(index++));
    LOG_GENERAL(INFO, "Retrieved DS leader ID: " << consensusLeaderID);
    string dataStr;

    while (true)
    {
        dataStr = m_dsCommitteeDB->Lookup(index++);

        if (dataStr.empty())
        {
            break;
        }

        dsCommittee->emplace_back(
            PubKey(vector<unsigned char>(dataStr.begin(),
                                         dataStr.begin() + PUB_KEY_SIZE),
                   0),
            Peer(vector<unsigned char>(dataStr.begin() + PUB_KEY_SIZE,
                                       dataStr.end()),
                 0));
        LOG_GENERAL(INFO,
                    "Retrieved DS committee: " << dsCommittee->back().first
                                               << ", "
                                               << dsCommittee->back().second);
    }

    return true;
}

bool BlockStorage::ResetDB(DBTYPE type)
{
    bool ret = false;
    switch (type)
    {
    case META:
        ret = m_metadataDB->ResetDB();
        break;
    case DS_BLOCK:
        ret = m_dsBlockchainDB->ResetDB();
        break;
    case TX_BLOCK:
        ret = m_txBlockchainDB->ResetDB();
        break;
    case TX_BODY:
        ret = m_txBodyDB->ResetDB();
        break;
    case TX_BODY_TMP:
        ret = m_txBodyTmpDB->ResetDB();
        break;
    case DS_COMMITTEE:
        ret = m_dsCommitteeDB->ResetDB();
        break;
    }
    if (!ret)
    {
        LOG_GENERAL(INFO, "FAIL: Reset DB " << type << " failed");
    }
    return ret;
}

std::vector<std::string> BlockStorage::GetDBName(DBTYPE type)
{
    std::vector<std::string> ret;
    switch (type)
    {
    case META:
        ret.push_back(m_metadataDB->GetDBName());
        break;
    case DS_BLOCK:
        ret.push_back(m_dsBlockchainDB->GetDBName());
        break;
    case TX_BLOCK:
        ret.push_back(m_txBlockchainDB->GetDBName());
        break;
    case TX_BODY:
        ret.push_back(m_txBodyDB->GetDBName());
        break;
    case TX_BODY_TMP:
        ret.push_back(m_txBodyTmpDB->GetDBName());
        break;
    case DS_COMMITTEE:
        ret.push_back(m_dsCommitteeDB->GetDBName());
        break;
    }

    return ret;
}

bool BlockStorage::ResetAll()
{
    if (!LOOKUP_NODE_MODE)
    {
        return ResetDB(META) && ResetDB(DS_BLOCK) && ResetDB(TX_BLOCK)
            && ResetDB(DS_COMMITTEE);
    }
    else // IS_LOOKUP_NODE
    {
        return ResetDB(META) && ResetDB(DS_BLOCK) && ResetDB(TX_BLOCK)
            && ResetDB(TX_BODY) && ResetDB(TX_BODY_TMP)
            && ResetDB(DS_COMMITTEE);
    }
}
