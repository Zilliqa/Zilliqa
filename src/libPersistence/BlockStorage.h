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

#include "ContractStorage.h"
#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"

typedef std::tuple<uint32_t, uint64_t, uint64_t, BlockType, BlockHash>
    BlockLink;

typedef std::shared_ptr<DSBlock> DSBlockSharedPtr;
typedef std::shared_ptr<TxBlock> TxBlockSharedPtr;
typedef std::shared_ptr<VCBlock> VCBlockSharedPtr;
typedef std::shared_ptr<FallbackBlockWShardingStructure> FallbackBlockSharedPtr;
typedef std::shared_ptr<BlockLink> BlockLinkSharedPtr;
typedef std::shared_ptr<MicroBlock> MicroBlockSharedPtr;
typedef std::shared_ptr<TransactionWithReceipt> TxBodySharedPtr;
typedef std::shared_ptr<std::pair<Address, Account>> StateSharedPtr;

struct DiagnosticDataNodes {
  DequeOfShard shards;
  DequeOfNode dsCommittee;
};

struct DiagnosticDataCoinbase {
  uint128_t nodeCount;  // Total num of nodes in the network for entire DS epoch
  uint128_t sigCount;   // Total num of signatories for the mined blocks across
                        // all Tx epochs
  uint32_t lookupCount;  // Num of lookup nodes
  uint128_t
      totalReward;  // Total reward based on COINBASE_REWARD_PER_DS and txn fees
  uint128_t baseReward;         // Base reward based on BASE_REWARD_IN_PERCENT
  uint128_t baseRewardEach;     // Base reward over nodeCount
  uint128_t lookupReward;       // LOOKUP_REWARD_IN_PERCENT percent of remaining
                                // reward after baseReward
  uint128_t rewardEachLookup;   // lookupReward over lookupCount
  uint128_t nodeReward;         // Remaining reward after lookupReward
  uint128_t rewardEach;         // nodeReward over sigCount
  uint128_t balanceLeft;        // Remaining reward after nodeReward
  PubKey luckyDrawWinnerKey;    // Recipient of balanceLeft (pubkey)
  Address luckyDrawWinnerAddr;  // Recipient of balanceLeft (address)

  bool operator==(const DiagnosticDataCoinbase& r) const {
    return std::tie(nodeCount, sigCount, lookupCount, totalReward, baseReward,
                    baseRewardEach, lookupReward, rewardEachLookup, nodeReward,
                    rewardEach, balanceLeft, luckyDrawWinnerKey,
                    luckyDrawWinnerAddr) ==
           std::tie(r.nodeCount, r.sigCount, r.lookupCount, r.totalReward,
                    r.baseReward, r.baseRewardEach, r.lookupReward,
                    r.rewardEachLookup, r.nodeReward, r.rewardEach,
                    r.balanceLeft, r.luckyDrawWinnerKey, r.luckyDrawWinnerAddr);
  }
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
  std::shared_ptr<LevelDB> m_tempStateDB;
  // m_diagnosticDBNodes is needed only for LOOKUP_NODE_MODE, but to make the
  // unit test and monitoring tools work with the default setting of
  // LOOKUP_NODE_MODE=false, we initialize it even if it's not a lookup node.
  std::shared_ptr<LevelDB> m_diagnosticDBNodes;
  std::shared_ptr<LevelDB> m_diagnosticDBCoinbase;
  std::shared_ptr<LevelDB> m_stateRootDB;
  /// used for historical data
  std::shared_ptr<LevelDB> m_txnHistoricalDB;
  std::shared_ptr<LevelDB> m_MBHistoricalDB;

  BlockStorage(const std::string& path = "", bool diagnostic = false)
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
        m_tempStateDB(std::make_shared<LevelDB>("tempState")),
        m_diagnosticDBNodes(
            std::make_shared<LevelDB>("diagnosticNodes", path, diagnostic)),
        m_diagnosticDBCoinbase(
            std::make_shared<LevelDB>("diagnosticCoinb", path, diagnostic)),
        m_stateRootDB(std::make_shared<LevelDB>("stateRoot")),
        m_diagnosticDBNodesCounter(0),
        m_diagnosticDBCoinbaseCounter(0) {
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
    TEMP_STATE,
    DIAGNOSTIC_NODES,
    DIAGNOSTIC_COINBASE,
    STATE_ROOT
  };

  /// Returns the singleton BlockStorage instance.
  static BlockStorage& GetBlockStorage(const std::string& path = "",
                                       bool diagnostic = false);

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

  bool GetHistoricalMicroBlock(const BlockHash& blockhash,
                               MicroBlockSharedPtr& microblock);

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

  /// Save state root
  bool PutStateRoot(const bytes& data);

  /// Save latest epoch when states were moved to disk
  bool PutLatestEpochStatesUpdated(const uint64_t& epochNum);

  /// Retrieve Last Transactions Trie Root Hash
  bool GetMetadata(MetaType type, bytes& data);

  // Retrieve the state root
  bool GetStateRoot(bytes& data);

  /// Save latest epoch when states were moved to disk
  bool GetLatestEpochStatesUpdated(uint64_t& epochNum);

  /// Save DS committee
  bool PutDSCommittee(const std::shared_ptr<DequeOfNode>& dsCommittee,
                      const uint16_t& consensusLeaderID);

  /// Retrieve DS committee
  bool GetDSCommittee(std::shared_ptr<DequeOfNode>& dsCommittee,
                      uint16_t& consensusLeaderID);

  /// Save shard structure
  bool PutShardStructure(const DequeOfShard& shards, const uint32_t myshardId);

  /// Retrieve shard structure
  bool GetShardStructure(DequeOfShard& shards);

  /// Save state delta
  bool PutStateDelta(const uint64_t& finalBlockNum, const bytes& stateDelta);

  /// Retrieve state delta
  bool GetStateDelta(const uint64_t& finalBlockNum, bytes& stateDelta);

  /// Write state to tempState in batch
  bool PutTempState(const std::unordered_map<Address, Account>& states);

  /// Get state from tempState in batch
  bool GetTempStateInBatch(leveldb::Iterator*& iter,
                           std::vector<StateSharedPtr>& states);

  /// Save data for diagnostic / monitoring purposes (nodes in network)
  bool PutDiagnosticDataNodes(const uint64_t& dsBlockNum,
                              const DequeOfShard& shards,
                              const DequeOfNode& dsCommittee);

  /// Save data for diagnostic / monitoring purposes (coinbase rewards)
  bool PutDiagnosticDataCoinbase(const uint64_t& dsBlockNum,
                                 const DiagnosticDataCoinbase& entry);

  /// Retrieve diagnostic data for specific block number (nodes in network)
  bool GetDiagnosticDataNodes(const uint64_t& dsBlockNum, DequeOfShard& shards,
                              DequeOfNode& dsCommittee);

  /// Retrieve diagnostic data for specific block number (coinbase rewards)
  bool GetDiagnosticDataCoinbase(const uint64_t& dsBlockNum,
                                 DiagnosticDataCoinbase& entry);

  /// Retrieve diagnostic data (nodes in network)
  void GetDiagnosticDataNodes(
      std::map<uint64_t, DiagnosticDataNodes>& diagnosticDataMap);

  /// Retrieve diagnostic data (coinbase rewards)
  void GetDiagnosticDataCoinbase(
      std::map<uint64_t, DiagnosticDataCoinbase>& diagnosticDataMap);

  /// Retrieve the number of entries in the diagnostic data db (nodes in
  /// network)
  unsigned int GetDiagnosticDataNodesCount();

  /// Retrieve the number of entries in the diagnostic data db (coinbase
  /// rewards)
  unsigned int GetDiagnosticDataCoinbaseCount();

  /// Delete the requested diagnostic data entry from the db (nodes in network)
  bool DeleteDiagnosticDataNodes(const uint64_t& dsBlockNum);

  /// Delete the requested diagnostic data entry from the db (coinbase rewards)
  bool DeleteDiagnosticDataCoinbase(const uint64_t& dsBlockNum);

  /// Clean a DB
  bool ResetDB(DBTYPE type);

  /// Refresh a DB
  bool RefreshDB(DBTYPE type);

  std::vector<std::string> GetDBName(DBTYPE type);

  /// Clean all DB
  bool ResetAll();

  /// Refresh all DB
  bool RefreshAll();

 private:
  std::mutex m_mutexDiagnostic;

  mutable std::shared_timed_mutex m_mutexMetadata;
  mutable std::shared_timed_mutex m_mutexDsBlockchain;
  mutable std::shared_timed_mutex m_mutexTxBlockchain;
  mutable std::shared_timed_mutex m_mutexMicroBlock;
  mutable std::shared_timed_mutex m_mutexDsCommittee;
  mutable std::shared_timed_mutex m_mutexVCBlock;
  mutable std::shared_timed_mutex m_mutexFallbackBlock;
  mutable std::shared_timed_mutex m_mutexBlockLink;
  mutable std::shared_timed_mutex m_mutexShardStructure;
  mutable std::shared_timed_mutex m_mutexStateDelta;
  mutable std::shared_timed_mutex m_mutexTempState;
  mutable std::shared_timed_mutex m_mutexTxBody;
  mutable std::shared_timed_mutex m_mutexTxBodyTmp;
  mutable std::shared_timed_mutex m_mutexStateRoot;
  mutable std::shared_timed_mutex m_mutexTxnHistorical;
  mutable std::shared_timed_mutex m_mutexMBHistorical;

  unsigned int m_diagnosticDBNodesCounter;
  unsigned int m_diagnosticDBCoinbaseCounter;
};

#endif  // BLOCKSTORAGE_H
