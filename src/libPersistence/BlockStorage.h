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

#ifndef ZILLIQA_SRC_LIBPERSISTENCE_BLOCKSTORAGE_H_
#define ZILLIQA_SRC_LIBPERSISTENCE_BLOCKSTORAGE_H_

#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include <Schnorr.h>
#include "libBlockchain/Block.h"
#include "libData/AccountData/Address.h"
#include "libData/MiningData/MinerInfo.h"

typedef std::tuple<uint32_t, uint64_t, uint64_t, BlockType, BlockHash>
    BlockLink;

class TransactionWithReceipt;
class LevelDB;

typedef std::shared_ptr<DSBlock> DSBlockSharedPtr;
typedef std::shared_ptr<TxBlock> TxBlockSharedPtr;
typedef std::shared_ptr<VCBlock> VCBlockSharedPtr;
typedef std::shared_ptr<BlockLink> BlockLinkSharedPtr;
typedef std::shared_ptr<MicroBlock> MicroBlockSharedPtr;
typedef std::shared_ptr<TransactionWithReceipt> TxBodySharedPtr;

struct DiagnosticDataNodes {
  DequeOfShardMembers shards;
  DequeOfNode dsCommittee;
};

struct DiagnosticDataCoinbase {
  uint128_t nodeCount;  // Total num of nodes in the network for entire DS epoch
  uint128_t sigCount;   // Total num of signatories for the mined blocks across
                        // all Tx epochs
  uint32_t lookupCount{};  // Num of lookup nodes
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
class BlockStorage : boost::noncopyable {
  std::shared_ptr<LevelDB> m_metadataDB;
  std::shared_ptr<LevelDB> m_dsBlockchainDB;
  std::shared_ptr<LevelDB> m_txBlockchainDB;
  std::shared_ptr<LevelDB> m_txBlockchainAuxDB;
  std::shared_ptr<LevelDB> m_txBlockHashToNumDB;
  std::vector<std::shared_ptr<LevelDB>> m_txBodyDBs;
  std::shared_ptr<LevelDB> m_txBodyOrigDB;
  std::shared_ptr<LevelDB> m_txEpochDB;
  std::shared_ptr<LevelDB> m_txTraceDB;
  std::shared_ptr<LevelDB> m_otterTraceDB;
  std::shared_ptr<LevelDB> m_otterTxAddressMappingDB;
  std::shared_ptr<LevelDB> m_otterAddressNonceLookup;
  std::vector<std::shared_ptr<LevelDB>> m_microBlockDBs;
  std::shared_ptr<LevelDB> m_microBlockOrigDB;
  std::shared_ptr<LevelDB> m_microBlockKeyDB;
  std::shared_ptr<LevelDB> m_dsCommitteeDB;
  std::shared_ptr<LevelDB> m_VCBlockDB;
  std::shared_ptr<LevelDB> m_blockLinkDB;
  std::shared_ptr<LevelDB> m_shardStructureDB;
  std::shared_ptr<LevelDB> m_stateDeltaDB;
  std::shared_ptr<LevelDB> m_processedTxnTmpDB;
  // m_diagnosticDBNodes is needed only for LOOKUP_NODE_MODE, but to make the
  // unit test and monitoring tools work with the default setting of
  // LOOKUP_NODE_MODE=false, we initialize it even if it's not a lookup node.
  std::shared_ptr<LevelDB> m_diagnosticDBNodes;
  std::shared_ptr<LevelDB> m_diagnosticDBCoinbase;
  std::shared_ptr<LevelDB> m_stateRootDB;
  /// used for miner nodes (DS committee) retrieval
  std::shared_ptr<LevelDB> m_minerInfoDSCommDB;
  /// used for miner nodes (shards) retrieval
  std::shared_ptr<LevelDB> m_minerInfoShardsDB;
  /// used for extseed pub key storage and retrieval
  std::shared_ptr<LevelDB> m_extSeedPubKeysDB;
  /// stores the hash of the transaction which created a contract
  std::shared_ptr<LevelDB> m_contractCreatorDB;

  BlockStorage(const std::string& path = "", bool diagnostic = false)
      : m_diagnosticDBNodesCounter(0), m_diagnosticDBCoinbaseCounter(0) {
    Initialize(path, diagnostic);
  };
  ~BlockStorage() = default;
  bool PutBlock(const uint64_t& blockNum, const zbytes& body,
                const BlockType& blockType);

 public:
  enum DBTYPE {
    META = 0x00,
    DS_BLOCK,
    TX_BLOCK,
    TX_BODY,
    MICROBLOCK,
    DS_COMMITTEE,
    VC_BLOCK,
    BLOCKLINK,
    SHARD_STRUCTURE,
    STATE_DELTA,
    DIAGNOSTIC_NODES,
    DIAGNOSTIC_COINBASE,
    STATE_ROOT,
    PROCESSED_TEMP,
    MINER_INFO_DSCOMM,
    MINER_INFO_SHARDS,
    EXTSEED_PUBKEYS,
    TX_BLOCK_HASH_TO_NUM,
    TX_BLOCK_AUX
  };

  /// Returns the singleton BlockStorage instance.
  static BlockStorage& GetBlockStorage(const std::string& path = "",
                                       bool diagnostic = false);

  void Initialize(const std::string& path = "", bool diagnostic = false);

  /// Adds a DS block to storage.
  bool PutDSBlock(const uint64_t& blockNum, const zbytes& body);
  bool PutVCBlock(const BlockHash& blockhash, const zbytes& body);
  bool PutBlockLink(const uint64_t& index, const zbytes& body);

  /// Adds a Tx block to storage.
  bool PutTxBlock(const TxBlockHeader& header, const zbytes& body);

  // /// Adds a micro block to storage.
  bool PutMicroBlock(const BlockHash& blockHash, const uint64_t& epochNum,
                     const uint32_t& shardID, const zbytes& body);

  /// Adds a transaction body to storage.
  bool PutTxBody(const uint64_t& epochNum, const dev::h256& key,
                 const zbytes& body);

  bool PutProcessedTxBodyTmp(const dev::h256& key, const zbytes& body);

  /// Retrieves the requested DS block.
  bool GetDSBlock(const uint64_t& blockNum, DSBlockSharedPtr& block);

  bool GetVCBlock(const BlockHash& blockhash, VCBlockSharedPtr& block);
  bool GetBlockLink(const uint64_t& index, BlockLinkSharedPtr& block);
  /// Retrieves the requested Tx block.
  bool GetTxBlock(const uint64_t& blockNum, TxBlockSharedPtr& block) const;
  bool GetTxBlock(const BlockHash& blockhash, TxBlockSharedPtr& block) const;

  bool GetLatestTxBlock(TxBlockSharedPtr& block);

  bool CheckTxBody(const dev::h256& key);

  bool ReleaseDB();

  /// Retrieves the requested Micro block using hash
  bool GetMicroBlock(const BlockHash& blockHash,
                     MicroBlockSharedPtr& microblock);
  /// Retrieves the requested Micro block using epochNum+shardID
  bool GetMicroBlock(const uint64_t& epochNum, const uint32_t& shardID,
                     MicroBlockSharedPtr& microblock);

  bool CheckMicroBlock(const BlockHash& blockHash);

  /// Retrieves the range Micro blocks
  bool GetRangeMicroBlocks(const uint64_t lowEpochNum,
                           const uint64_t hiEpochNum, const uint32_t loShardId,
                           const uint32_t hiShardId,
                           std::list<MicroBlockSharedPtr>& blocks);

  /// Retrieves the requested transaction body.
  bool GetTxBody(const dev::h256& key, TxBodySharedPtr& body);

  /// Retrieves the requested transaction trace.
  bool PutTxTrace(const dev::h256& key, const std::string& trace);
  bool GetTxTrace(const dev::h256& key, std::string& trace);
  std::shared_ptr<LevelDB> GetTxTraceDb();

  /// Retrieves the requested transaction trace for otterscan.
  bool PutOtterTrace(const dev::h256& key, const std::string& trace);
  bool GetOtterTrace(const dev::h256& key, std::string& trace);

  /// Retrieves the mappings of address touched to TX.
  bool PutOtterTxAddressMapping(const dev::h256& txId,
                                const std::set<std::string>& addresses,
                                const uint64_t& blocknum);
  std::vector<std::string> GetOtterTxAddressMapping(std::string address,
                                                    unsigned long blockNumber,
                                                    unsigned long pageSize,
                                                    bool before, bool& wasMore);

  /// Retrieves the mappings of address touched to TX.
  bool PutOtterAddressNonceLookup(const dev::h256& txId, uint64_t nonce,
                                  std::string address);
  std::string GetOtterAddressNonceLookup(std::string address, uint64_t nonce);

  /// Deletes the requested Tx block
  bool DeleteTxBlock(const uint64_t& blocknum);

  bool DeleteStateDelta(const uint64_t& finalBlockNum);

  /// Retrieves all the DSBlocks
  bool GetAllDSBlocks(std::list<DSBlockSharedPtr>& blocks);

  /// Retrieves all the TxBlocks
  bool GetAllTxBlocks(std::deque<TxBlockSharedPtr>& blocks);

  /// Retrieves all the VCBlocks
  bool GetAllVCBlocks(std::list<VCBlockSharedPtr>& blocks);

  /// Retrieve all the blocklink
  bool GetAllBlockLink(std::list<BlockLink>& blocklinks);

  /// Put extseed public key to storage
  bool PutExtSeedPubKey(const PubKey& pubK);

  /// Delete extseed public key from storage
  bool DeleteExtSeedPubKey(const PubKey& pubK);

  /// Retrieve all the extseed pubkeys
  bool GetAllExtSeedPubKeys(std::unordered_set<PubKey>& pubKeys);

  /// Save Last Transactions Trie Root Hash
  bool PutMetadata(MetaType type, const zbytes& data);

  /// Save state root
  bool PutStateRoot(const zbytes& data);

  /// Save latest epoch when states were moved to disk
  bool PutLatestEpochStatesUpdated(const uint64_t& epochNum);

  /// Save the latest epoch being fully completed
  bool PutEpochFin(const uint64_t& epochNum);

  /// Retrieve Last Transactions Trie Root Hash
  bool GetMetadata(MetaType type, zbytes& data, bool muteLog = false);

  // Retrieve the state root
  bool GetStateRoot(zbytes& data);

  /// Save latest epoch when states were moved to disk
  bool GetLatestEpochStatesUpdated(uint64_t& epochNum);

  /// Get the latest epoch being fully completed
  bool GetEpochFin(uint64_t& epochNum);

  /// Save DS committee
  bool PutDSCommittee(const std::shared_ptr<DequeOfNode>& dsCommittee,
                      const uint16_t& consensusLeaderID);

  /// Retrieve DS committee
  bool GetDSCommittee(std::shared_ptr<DequeOfNode>& dsCommittee,
                      uint16_t& consensusLeaderID);

  /// Save shard structure
  bool PutShardStructure(const DequeOfShardMembers& shards,
                         const uint32_t myshardId);

  /// Retrieve shard structure
  bool GetShardStructure(DequeOfShardMembers& shards);

  /// Save state delta
  bool PutStateDelta(const uint64_t& finalBlockNum, const zbytes& stateDelta);

  /// Retrieve state delta
  bool GetStateDelta(const uint64_t& finalBlockNum, zbytes& stateDelta);

  /// Save data for diagnostic / monitoring purposes (nodes in network)
  bool PutDiagnosticDataNodes(const uint64_t& dsBlockNum,
                              const DequeOfShardMembers& shards,
                              const DequeOfNode& dsCommittee);

  /// Save data for diagnostic / monitoring purposes (coinbase rewards)
  bool PutDiagnosticDataCoinbase(const uint64_t& dsBlockNum,
                                 const DiagnosticDataCoinbase& entry);

  /// Retrieve diagnostic data for specific block number (nodes in network)
  bool GetDiagnosticDataNodes(const uint64_t& dsBlockNum,
                              DequeOfShardMembers& shards,
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

  /// Adds miner info (DS committee) to storage
  bool PutMinerInfoDSComm(const uint64_t& dsBlockNum,
                          const MinerInfoDSComm& entry);

  /// Retrieves the requested miner info (DS committee)
  bool GetMinerInfoDSComm(const uint64_t& dsBlockNum, MinerInfoDSComm& entry);

  /// Adds miner info (shards) to storage
  bool PutMinerInfoShards(const uint64_t& dsBlockNum,
                          const MinerInfoShards& entry);

  /// Retrieves the requested miner info (shards)
  bool GetMinerInfoShards(const uint64_t& dsBlockNum, MinerInfoShards& entry);

  /// Put contract creation transaction hash
  bool PutContractCreator(const dev::h160 address, const dev::h256 txnHash);

  /// Get a contract creation transaction hash
  dev::h256 GetContractCreator(const dev::h160 address);

  /// Clean a DB
  bool ResetDB(DBTYPE type);

  /// Refresh a DB
  bool RefreshDB(DBTYPE type);

  /// Clean all DB
  bool ResetAll();

  /// Refresh all DB
  bool RefreshAll();

 private:
  std::mutex m_mutexDiagnostic;

  mutable std::shared_timed_mutex m_mutexMetadata;
  mutable std::shared_timed_mutex m_mutexDsBlockchain;
  mutable std::shared_timed_mutex m_mutexTxBlockchain;
  mutable std::mutex m_mutexMicroBlock;
  mutable std::shared_timed_mutex m_mutexDsCommittee;
  mutable std::shared_timed_mutex m_mutexVCBlock;
  mutable std::shared_timed_mutex m_mutexBlockLink;
  mutable std::shared_timed_mutex m_mutexShardStructure;
  mutable std::shared_timed_mutex m_mutexStateDelta;
  mutable std::shared_timed_mutex m_mutexTempState;
  mutable std::mutex m_mutexTxBody;
  mutable std::shared_timed_mutex m_mutexStateRoot;
  mutable std::shared_timed_mutex m_mutexProcessTx;
  mutable std::shared_timed_mutex m_mutexMinerInfoDSComm;
  mutable std::shared_timed_mutex m_mutexMinerInfoShards;
  mutable std::shared_timed_mutex m_mutexExtSeedPubKeys;
  mutable std::mutex m_contractCreatorMutex;

  unsigned int m_diagnosticDBNodesCounter;
  unsigned int m_diagnosticDBCoinbaseCounter;

  std::shared_ptr<LevelDB> GetMicroBlockDB(const uint64_t& epochNum);
  std::shared_ptr<LevelDB> GetTxBodyDB(const uint64_t& epochNum);
  void BuildHashToNumberMappingForTxBlocks();
};

#endif  // ZILLIQA_SRC_LIBPERSISTENCE_BLOCKSTORAGE_H_
