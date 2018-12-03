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
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"

typedef std::tuple<uint64_t, uint64_t, BlockType, BlockHash> BlockLink;

typedef std::shared_ptr<DSBlock> DSBlockSharedPtr;
typedef std::shared_ptr<TxBlock> TxBlockSharedPtr;
typedef std::shared_ptr<VCBlock> VCBlockSharedPtr;
typedef std::shared_ptr<FallbackBlockWShardingStructure> FallbackBlockSharedPtr;
typedef std::shared_ptr<BlockLink> BlockLinkSharedPtr;
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
  std::shared_ptr<LevelDB> m_VCBlockDB;
  std::shared_ptr<LevelDB> m_fallbackBlockDB;
  std::shared_ptr<LevelDB> m_blockLinkDB;
  std::shared_ptr<LevelDB> m_shardStructureDB;
  std::shared_ptr<LevelDB> m_stateDeltaDB;

  BlockStorage()
      : m_metadataDB(std::make_shared<LevelDB>("metadata")),
        m_dsBlockchainDB(std::make_shared<LevelDB>("dsBlocks")),
        m_txBlockchainDB(std::make_shared<LevelDB>("txBlocks")),
        m_microBlockDB(std::make_shared<LevelDB>("microBlocks")),
        m_dsCommitteeDB(std::make_shared<LevelDB>("dsCommittee")),
        m_VCBlockDB(std::make_shared<LevelDB>("VCBlocks")),
        m_fallbackBlockDB(std::make_shared<LevelDB>("fallbackBlocks")),
        m_blockLinkDB(std::make_shared<LevelDB>("blockLinks")),
        m_shardStructureDB(std::make_shared<LevelDB>("shardStructure")),
        m_stateDeltaDB(std::make_shared<LevelDB>("stateDelta")) {
    if (LOOKUP_NODE_MODE) {
      m_txBodyDB = std::make_shared<LevelDB>("txBodies");
      m_txBodyTmpDB = std::make_shared<LevelDB>("txBodiesTmp");
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
    VC_BLOCK,
    FB_BLOCK,
    BLOCKLINK,
    SHARD_STRUCTURE,
    STATE_DELTA
  };

  /// Returns the singleton BlockStorage instance.
  static BlockStorage& GetBlockStorage();

  /// Get the size of current TxBodyDB
  unsigned int GetTxBodyDBSize();

  /// Adds a DS block to storage.
  bool PutDSBlock(const uint64_t& blockNum,
                  const std::vector<unsigned char>& body);
  bool PutVCBlock(const BlockHash& blockhash,
                  const std::vector<unsigned char>& body);
  bool PutFallbackBlock(const BlockHash& blockhash,
                        const std::vector<unsigned char>& body);
  bool PutBlockLink(const uint64_t& index,
                    const std::vector<unsigned char>& body);

  /// Adds a Tx block to storage.
  bool PutTxBlock(const uint64_t& blockNum,
                  const std::vector<unsigned char>& body);

  // /// Adds a micro block to storage.
  bool PutMicroBlock(const BlockHash& blockHash,
                     const std::vector<unsigned char>& body);

  /// Adds a transaction body to storage.
  bool PutTxBody(const dev::h256& key, const std::vector<unsigned char>& body);

  /// Retrieves the requested DS block.
  bool GetDSBlock(const uint64_t& blockNum, DSBlockSharedPtr& block);

  bool GetVCBlock(const BlockHash& blockhash, VCBlockSharedPtr& block);
  bool GetFallbackBlock(
      const BlockHash& blockhash,
      FallbackBlockSharedPtr& fallbackblockwshardingstructure);
  bool GetBlockLink(const uint64_t& index, BlockLinkSharedPtr& block);
  /// Retrieves the requested Tx block.
  bool GetTxBlock(const uint64_t& blockNum, TxBlockSharedPtr& block);

  // /// Retrieves the requested Micro block
  bool GetMicroBlock(const BlockHash& blockHash,
                     MicroBlockSharedPtr& microblock);

  // /// Retrieves the range Micro blocks
  bool GetRangeMicroBlocks(const uint64_t lowEpochNum,
                           const uint64_t hiEpochNum, const uint32_t loShardId,
                           const uint32_t hiShardId,
                           std::list<MicroBlockSharedPtr>& blocks);

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

  bool DeleteVCBlock(const BlockHash& blockhash);

  bool DeleteFallbackBlock(const BlockHash& blockhash);

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

  /// Retrieve all the blocklink
  bool GetAllBlockLink(std::list<BlockLink>& blocklinks);

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

  /// Save shard structure
  bool PutShardStructure(const DequeOfShard& shards, const uint32_t myshardId);

  /// Retrieve shard structure
  bool GetShardStructure(DequeOfShard& shards);

  /// Save state delta
  bool PutStateDelta(const uint64_t& finalBlockNum,
                     const std::vector<unsigned char>& stateDelta);

  /// Retrieve state delta
  bool GetStateDelta(const uint64_t& finalBlockNum,
                     std::vector<unsigned char>& stateDelta);

  /// Clean a DB
  bool ResetDB(DBTYPE type);

  std::vector<std::string> GetDBName(DBTYPE type);

  /// Clean all DB
  bool ResetAll();
};

#endif  // BLOCKSTORAGE_H
