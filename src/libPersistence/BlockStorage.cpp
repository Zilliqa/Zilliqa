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

#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <leveldb/db.h>

#include "BlockStorage.h"

using namespace std;

BlockStorage & BlockStorage::GetBlockStorage()
{
    static BlockStorage bs;
    return bs;
}

bool BlockStorage::PutBlock(const boost::multiprecision::uint256_t & blockNum, 
    const vector<unsigned char> & body, const BlockType & blockType)
{
    int ret;
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

bool BlockStorage::PutDSBlock(const boost::multiprecision::uint256_t & blockNum, 
    const vector<unsigned char> & body)
{
    return PutBlock(blockNum, body, BlockType::DS);
}

bool BlockStorage::PutTxBlock(const boost::multiprecision::uint256_t & blockNum, 
    const vector<unsigned char> & body)
{
    return PutBlock(blockNum, body, BlockType::Tx);
}

bool BlockStorage::GetDSBlock(const boost::multiprecision::uint256_t & blockNum, 
    DSBlockSharedPtr & block)
{
    string blockString = m_dsBlockchainDB.Lookup(blockNum);

    if(blockString.empty())
    {
        return false;
    }
    
    LOG_MESSAGE(blockString);
    LOG_MESSAGE(blockString.length());
    const unsigned char* raw_memory = reinterpret_cast<const unsigned char*>(blockString.c_str());
    block = DSBlockSharedPtr( new DSBlock(std::vector<unsigned char>(raw_memory, 
                                          raw_memory + blockString.size()), 0) );
    return true;
}

bool BlockStorage::GetTxBlock(const boost::multiprecision::uint256_t & blockNum, 
    TxBlockSharedPtr & block)
{
    string blockString = m_txBlockchainDB.Lookup(blockNum);
 
    if(blockString.empty())
    {
        return false;
    }
 
    const unsigned char* raw_memory = reinterpret_cast<const unsigned char*>(blockString.c_str());
    block = TxBlockSharedPtr( new TxBlock(std::vector<unsigned char>(raw_memory, 
                                          raw_memory + blockString.size()), 0) );
    return true;
}

bool BlockStorage::PutTxBody(const dev::h256 & key, const vector<unsigned char> & body)
{
    int ret = m_txBodyDB.Insert(key, body);
    return (ret == 0);
}

bool BlockStorage::GetTxBody(const dev::h256 & key, TxBodySharedPtr & body)
{
    string bodyString = m_txBodyDB.Lookup(key);
    
    if(bodyString.empty())
    {
        return false;
    }
    
    const unsigned char* raw_memory = reinterpret_cast<const unsigned char*>(bodyString.c_str());
    body = TxBodySharedPtr( new Transaction(std::vector<unsigned char>(raw_memory, 
                                            raw_memory + bodyString.size()), 0) );
    return true;
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