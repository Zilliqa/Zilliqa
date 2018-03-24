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

#ifndef BLOCKSTORAGE_H
#define BLOCKSTORAGE_H

#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "depends/libDatabase/LevelDB.h"
#include "libData/BlockData/Block.h"

typedef std::shared_ptr<DSBlock> DSBlockSharedPtr; 
typedef std::shared_ptr<TxBlock> TxBlockSharedPtr;
typedef std::shared_ptr<MicroBlock> MicroBlockSharedPtr;
typedef std::shared_ptr<Transaction> TxBodySharedPtr; 

/// Manages persistent storage of DS and Tx blocks.
class BlockStorage
{
    LevelDB m_metadataDB;
    LevelDB m_txBodyDB;
    LevelDB m_dsBlockchainDB;
    LevelDB m_txBlockchainDB;
    /// To keep track of the txBodies newly generated in the current unfinished DS Epoch
    LevelDB m_txBodyTmpDB;

    BlockStorage() : m_metadataDB("metadata"), m_txBodyDB("txBodies"), 
                     m_dsBlockchainDB("dsBlocks"), m_txBlockchainDB("txBlocks"),
                     m_txBodyTmpDB("txBodiesTmp") {};
    ~BlockStorage() = default;
    bool PutBlock(const boost::multiprecision::uint256_t & blockNum, 
                  const std::vector<unsigned char> & block, const BlockType & blockType);

public:
    enum DBTYPE {
        META = 0x00,
        DS_BLOCK,
        TX_BLOCK,
        TX_BODY,
        TX_BODY_TMP
    };

    /// Returns the singleton BlockStorage instance.
    static BlockStorage & GetBlockStorage();

    /// Adds a DS block to storage.
    bool PutDSBlock(const boost::multiprecision::uint256_t & blockNum, 
                    const std::vector<unsigned char> & block); 

    /// Adds a Tx block to storage.
    bool PutTxBlock(const boost::multiprecision::uint256_t & blockNum, 
                    const std::vector<unsigned char> & block); 

    // /// Adds a micro block to storage.
    // bool PutMicroBlock(const dev::h256 & key, const std::vector<unsigned char> & block);

    /// Adds a transaction body to storage.
    bool PutTxBody(const dev::h256 & key, const std::vector<unsigned char> & body);

    /// Retrieves the requested DS block.
    bool GetDSBlock(const boost::multiprecision::uint256_t & blocknum, DSBlockSharedPtr & block);

    /// Retrieves the requested Tx block.
    bool GetTxBlock(const boost::multiprecision::uint256_t & blocknum, TxBlockSharedPtr & block);

    // /// Retrieves the requested Micro block
    // bool GetMicroBlock(const dev::h256 & key, MicroBlockSharedPtr & block);

    /// Retrieves the requested transaction body.
    bool GetTxBody(const dev::h256 & key, TxBodySharedPtr & body);

    /// Deletes the requested DS block
    bool DeleteDSBlock(const boost::multiprecision::uint256_t & blocknum);

    /// Deletes the requested Tx block
    bool DeleteTxBlock(const boost::multiprecision::uint256_t & blocknum);

    // /// Deletes the requested Micro block
    // bool DeleteMicroBlock(const dev::h256 & key);

    /// Deletes the requested transaction body
    bool DeleteTxBody(const dev::h256 & key);

    // /// Adds a transaction body to storage.
    // bool PutTxBody(const std::string & key, const std::vector<unsigned char> & body);

    // /// Retrieves the requested transaction body.
    // void GetTxBody(const std::string & key, TxBodySharedPtr & body);

    /// Retrieves all the DSBlocks
    bool GetAllDSBlocks(std::list<DSBlockSharedPtr> & blocks);

    /// Retrieves all the TxBlocks
    bool GetAllTxBlocks(std::list<TxBlockSharedPtr> & blocks);

    /// Retrieves all the TxBodiesTmp
    bool GetAllTxBodiesTmp(std::list<TxnHash>& txnHashes);

    /// Save Last Transactions Trie Root Hash
    bool PutMetadata(MetaType type, const std::vector<unsigned char> & data);
    
    /// Retrieve Last Transactions Trie Root Hash
    bool GetMetadata(MetaType type, std::vector<unsigned char> & data);

    /// Deletes the requested metadata
    bool DeleteMetadata(const MetaType & metatype);

    /// Clean a DB
    bool ResetDB(DBTYPE type);
};

#endif // BLOCKSTORAGE_H