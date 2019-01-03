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

struct DiagnosticData {
  DequeOfShard shards;
  DequeOfDSNode dsCommittee;
};

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
  // m_diagnosticDB is needed only for LOOKUP_NODE_MODE, but to make the unit
  // test and monitoring tools work with the default setting of
  // LOOKUP_NODE_MODE=false, we initialize it even if it's not a lookup node.
  std::shared_ptr<LevelDB> m_diagnosticDB;
  /// used for historical data
  std::shared_ptr<LevelDB> m_historicalDB;

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
        m_stateDeltaDB(std::make_shared<LevelDB>("stateDelta")),
        m_diagnosticDB(std::make_shared<LevelDB>("diagnostic")),
        m_diagnosticDBCounter(0) {
    if (LOOKUP_NODE_MODE) {
      m_txBodyDB = std::make_shared<LevelDB>("txBodies");
      m_txBodyTmpDB = std::make_shared<LevelDB>("txBodiesTmp");
    }
  };
  ~BlockStorage() = default;
  bool PutBlock(const uint64_t& blockNum, const bytes& body,
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
    STATE_DELTA,
    DIAGNOSTIC
  };

  /// Returns the singleton BlockStorage instance.
  static BlockStorage& GetBlockStorage();

  /// Get the size of current TxBodyDB
  unsigned int GetTxBodyDBSize();

  /// Adds a DS block to storage.
  bool PutDSBlock(const uint64_t& blockNum, const bytes& body);
  bool PutVCBlock(const BlockHash& blockhash, const bytes& body);
  bool PutFallbackBlock(const BlockHash& blockhash, const bytes& body);
  bool PutBlockLink(const uint64_t& index, const bytes& body);

  bool InitiateHistoricalDB(const std::string& path);

  /// Adds a Tx block to storage.
  bool PutTxBlock(const uint64_t& blockNum, const bytes& body);

  // /// Adds a micro block to storage.
  bool PutMicroBlock(const BlockHash& blockHash, const bytes& body);

  /// Adds a transaction body to storage.
  bool PutTxBody(const dev::h256& key, const bytes& body);

  /// Retrieves the requested DS block.
  bool GetDSBlock(const uint64_t& blockNum, DSBlockSharedPtr& block);

  bool GetVCBlock(const BlockHash& blockhash, VCBlockSharedPtr& block);
  bool GetFallbackBlock(
      const BlockHash& blockhash,
      FallbackBlockSharedPtr& fallbackblockwshardingstructure);
  bool GetBlockLink(const uint64_t& index, BlockLinkSharedPtr& block);
  /// Retrieves the requested Tx block.
  bool GetTxBlock(const uint64_t& blockNum, TxBlockSharedPtr& block);

  bool ReleaseDB();

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

  bool GetTxnFromHistoricalDB(const dev::h256& key, TxBodySharedPtr& body);

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
  // bool PutTxBody(const std::string & key, const bytes &
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
  bool PutMetadata(MetaType type, const bytes& data);

  /// Retrieve Last Transactions Trie Root Hash
  bool GetMetadata(MetaType type, bytes& data);

  /// Save DS committee
  bool PutDSCommittee(const std::shared_ptr<DequeOfDSNode>& dsCommittee,
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
  bool PutStateDelta(const uint64_t& finalBlockNum, const bytes& stateDelta);

  /// Retrieve state delta
  bool GetStateDelta(const uint64_t& finalBlockNum, bytes& stateDelta);

  /// Save data for diagnostic / monitoring purposes
  bool PutDiagnosticData(const uint64_t& dsBlockNum, const DequeOfShard& shards,
                         const DequeOfDSNode& dsCommittee);

  /// Retrieve diagnostic data for specific block number
  bool GetDiagnosticData(const uint64_t& dsBlockNum, DequeOfShard& shards,
                         DequeOfDSNode& dsCommittee);

  /// Retrieve diagnostic data
  void GetDiagnosticData(std::map<uint64_t, DiagnosticData>& diagnosticDataMap);

  /// Retrieve the number of entries in the diagnostic data db
  unsigned int GetDiagnosticDataCount();

  /// Delete the requested diagnostic data entry from the db
  bool DeleteDiagnosticData(const uint64_t& dsBlockNum);

  /// Clean a DB
  bool ResetDB(DBTYPE type);

  std::vector<std::string> GetDBName(DBTYPE type);

  /// Clean all DB
  bool ResetAll();

 private:
  std::mutex m_mutexMetadata;
  std::mutex m_mutexDsBlockchain;
  std::mutex m_mutexTxBlockchain;
  std::mutex m_mutexMicroBlock;
  std::mutex m_mutexDsCommittee;
  std::mutex m_mutexVCBlock;
  std::mutex m_mutexFallbackBlock;
  std::mutex m_mutexBlockLink;
  std::mutex m_mutexShardStructure;
  std::mutex m_mutexStateDelta;
  std::mutex m_mutexTxBody;
  std::mutex m_mutexTxBodyTmp;
  std::mutex m_mutexDiagnostic;

  unsigned int m_diagnosticDBCounter;
};

#endif  // BLOCKSTORAGE_H
