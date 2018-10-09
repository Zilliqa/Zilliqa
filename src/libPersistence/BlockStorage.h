/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef BLOCKSTORAGE_H
#define BLOCKSTORAGE_H

#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"
#include "libData/BlockData/Block.h"

typedef std::shared_ptr<DSBlock> DSBlockSharedPtr;
typedef std::shared_ptr<TxBlock> TxBlockSharedPtr;
typedef std::shared_ptr<VCBlock> VCBlockSharedPtr;
typedef std::shared_ptr<MicroBlock> MicroBlockSharedPtr;
typedef std::shared_ptr<TransactionWithReceipt> TxBodySharedPtr;

/// Manages persistent storage of DS and Tx blocks.
class BlockStorage : public Singleton<BlockStorage> {
  std::shared_ptr<LevelDB> m_metadataDB;
  std::shared_ptr<LevelDB> m_dsBlockchainDB;
  std::shared_ptr<LevelDB> m_txBlockchainDB;
  std::shared_ptr<LevelDB> m_txBodyDB;
  std::shared_ptr<LevelDB> m_microBlockDB;
  std::shared_ptr<LevelDB> m_txBodyTmpDB;
  std::shared_ptr<LevelDB> m_dsCommitteeDB;

  BlockStorage()
      : m_metadataDB(std::make_shared<LevelDB>("metadata")),
        m_dsBlockchainDB(std::make_shared<LevelDB>("dsBlocks")),
        m_txBlockchainDB(std::make_shared<LevelDB>("txBlocks")),
        m_dsCommitteeDB(std::make_shared<LevelDB>("dsCommittee")) {
    if (LOOKUP_NODE_MODE) {
      m_txBodyDB = std::make_shared<LevelDB>("txBodies");
      m_txBodyTmpDB = std::make_shared<LevelDB>("txBodiesTmp");
      m_microBlockDB = std::make_shared<LevelDB>("microBlocks");
    }
  };
  ~BlockStorage() = default;
  bool PutBlock(const uint64_t& blockNum,
                const std::vector<unsigned char>& body,
                const BlockType& blockType);

 public:
  enum DBTYPE {
    META = 0x00,
    DS_BLOCK,
    TX_BLOCK,
    TX_BODY,
    TX_BODY_TMP,
    MICROBLOCK,
    DS_COMMITTEE,
  };

  /// Returns the singleton BlockStorage instance.
  static BlockStorage& GetBlockStorage();

  /// Get the size of current TxBodyDB
  unsigned int GetTxBodyDBSize();

  /// Adds a DS block to storage.
  bool PutDSBlock(const uint64_t& blockNum,
                  const std::vector<unsigned char>& body);

  /// Adds a Tx block to storage.
  bool PutTxBlock(const uint64_t& blockNum,
                  const std::vector<unsigned char>& body);

  // /// Adds a micro block to storage.
  bool PutMicroBlock(const uint64_t& blocknum, const uint32_t& shardId,
                     const std::vector<unsigned char>& body);

  /// Adds a transaction body to storage.
  bool PutTxBody(const dev::h256& key, const std::vector<unsigned char>& body);

  /// Retrieves the requested DS block.
  bool GetDSBlock(const uint64_t& blockNum, DSBlockSharedPtr& block);

  /// Retrieves the requested Tx block.
  bool GetTxBlock(const uint64_t& blockNum, TxBlockSharedPtr& block);

  // /// Retrieves the requested Micro block
  bool GetMicroBlock(const uint64_t& blocknum, const uint32_t& shardId,
                     MicroBlockSharedPtr& microblock);

  /// Retrieves the requested transaction body.
  bool GetTxBody(const dev::h256& key, TxBodySharedPtr& body);

  /// Deletes the requested DS block
  bool DeleteDSBlock(const uint64_t& blocknum);

  /// Deletes the requested Tx block
  bool DeleteTxBlock(const uint64_t& blocknum);

  // /// Deletes the requested Micro block
  // bool DeleteMicroBlock(const dev::h256 & key);

  /// Deletes the requested transaction body
  bool DeleteTxBody(const dev::h256& key);

  // /// Adds a transaction body to storage.
  // bool PutTxBody(const std::string & key, const std::vector<unsigned char> &
  // body);

  // /// Retrieves the requested transaction body.
  // void GetTxBody(const std::string & key, TxBodySharedPtr & body);

  /// Retrieves all the DSBlocks
  bool GetAllDSBlocks(std::list<DSBlockSharedPtr>& blocks);

  /// Retrieves all the TxBlocks
  bool GetAllTxBlocks(std::list<TxBlockSharedPtr>& blocks);

  /// Retrieves all the TxBodiesTmp
  bool GetAllTxBodiesTmp(std::list<TxnHash>& txnHashes);

  /// Save Last Transactions Trie Root Hash
  bool PutMetadata(MetaType type, const std::vector<unsigned char>& data);

  /// Retrieve Last Transactions Trie Root Hash
  bool GetMetadata(MetaType type, std::vector<unsigned char>& data);

  /// Save DS committee
  bool PutDSCommittee(
      const std::shared_ptr<std::deque<std::pair<PubKey, Peer>>>& dsCommittee,
      const uint16_t& consensusLeaderID);

  /// Retrieve DS committee
  bool GetDSCommittee(
      std::shared_ptr<std::deque<std::pair<PubKey, Peer>>>& dsCommittee,
      uint16_t& consensusLeaderID);

  /// Clean a DB
  bool ResetDB(DBTYPE type);

  std::vector<std::string> GetDBName(DBTYPE type);

  /// Clean all DB
  bool ResetAll();
};

#endif  // BLOCKSTORAGE_H
