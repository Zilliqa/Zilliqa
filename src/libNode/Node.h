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
#ifndef __NODE_H__
#define __NODE_H__

#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "common/Constants.h"
#include "common/Executable.h"
#include "common/MempoolEnum.h"
#include "depends/common/FixedHash.h"
#include "libConsensus/Consensus.h"
#include "libData/AccountData/MBnForwardedTxnEntry.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountData/TxnPool.h"
#include "libData/BlockData/Block.h"
#include "libLookup/Synchronizer.h"
#include "libNetwork/DataSender.h"
#include "libNetwork/P2PComm.h"
#include "libPersistence/BlockStorage.h"

class Mediator;
class Retriever;

/// Implements PoW submission and sharding node functionality.
class Node : public Executable {
  enum Action {
    STARTPOW = 0x00,
    PROCESS_DSBLOCK,
    PROCESS_MICROBLOCKCONSENSUS,
    PROCESS_FINALBLOCK,
    PROCESS_TXNBODY,
    PROCESS_FALLBACKCONSENSUS,
    PROCESS_FALLBACKBLOCK,
    NUM_ACTIONS
  };

  enum SUBMITTRANSACTIONTYPE : unsigned char { MISSINGTXN = 0x01 };

  enum REJOINTYPE : unsigned char {
    ATFINALBLOCK = 0x00,
    ATNEXTROUND = 0x01,
    ATSTATEROOT = 0x02,
    ATDSCONSENSUS = 0x03,     // For DS Rejoin
    ATFINALCONSENSUS = 0x04,  // For DS Rejoin
  };

  enum LEGITIMACYRESULT : unsigned char {
    SUCCESS = 0x00,
    MISSEDTXN,
    WRONGORDER,
    SERIALIZATIONERROR,
    DESERIALIZATIONERROR
  };

  Mediator& m_mediator;

  Synchronizer m_synchronizer;

  // DS block information
  std::mutex m_mutexConsensus;

  // Sharding information
  std::atomic<uint32_t> m_numShards;

  // pre-generated addresses
  std::vector<Address> m_populatedAddresses;

  // Consensus variables
  std::mutex m_mutexProcessConsensusMessage;
  std::condition_variable cv_processConsensusMessage;
  std::mutex m_MutexCVMicroblockConsensus;
  std::mutex m_MutexCVMicroblockConsensusObject;
  std::condition_variable cv_microblockConsensusObject;
  std::atomic<uint16_t> m_consensusMyID;
  std::atomic<uint16_t> m_consensusLeaderID;

  std::mutex m_MutexCVFBWaitMB;
  std::condition_variable cv_FBWaitMB;

  /// DSBlock Timer Vars
  std::mutex m_mutexCVWaitDSBlock;
  std::condition_variable cv_waitDSBlock;

  // Final Block Buffer for seed node
  std::vector<bytes> m_seedTxnBlksBuffer;
  std::mutex m_mutexSeedTxnBlksBuffer;

  // Persistence Retriever
  std::shared_ptr<Retriever> m_retriever;

  bytes m_consensusBlockHash;
  std::pair<uint64_t, CoSignatures> m_lastMicroBlockCoSig;
  std::mutex m_mutexMicroBlock;

  const static uint32_t RECVTXNDELAY_MILLISECONDS = 3000;
  const static unsigned int GOSSIP_RATE = 48;

  // Transactions information
  std::atomic<bool> m_txn_distribute_window_open;
  std::mutex m_mutexCreatedTransactions;
  TxnPool m_createdTxns, t_createdTxns;

  std::shared_timed_mutex mutable m_unconfirmedTxnsMutex;
  std::unordered_map<TxnHash, PoolTxnStatus> m_unconfirmedTxns;

  std::vector<TxnHash> m_expectedTranOrdering;
  std::mutex m_mutexProcessedTransactions;
  std::unordered_map<uint64_t,
                     std::unordered_map<TxnHash, TransactionWithReceipt>>
      m_processedTransactions;
  std::unordered_map<TxnHash, TransactionWithReceipt> t_processedTransactions;
  // operates under m_mutexProcessedTransaction
  std::vector<TxnHash> m_TxnOrder;

  uint64_t m_gasUsedTotal = 0;
  uint128_t m_txnFees = 0;

  // std::mutex m_mutexCommittedTransactions;
  // std::unordered_map<uint64_t, std::list<TransactionWithReceipt>>
  //     m_committedTransactions;

  std::mutex m_mutexMBnForwardedTxnBuffer;
  std::unordered_map<uint64_t, std::vector<MBnForwardedTxnEntry>>
      m_mbnForwardedTxnBuffer;

  std::mutex m_mutexTxnPacketBuffer;
  std::vector<bytes> m_txnPacketBuffer;

  // txn proc timeout related
  std::mutex m_mutexCVTxnProcFinished;
  std::condition_variable cv_TxnProcFinished;

  std::mutex m_mutexMicroBlockConsensusBuffer;
  std::unordered_map<uint32_t, VectorOfNodeMsg> m_microBlockConsensusBuffer;

  // Fallback Consensus
  std::mutex m_mutexFallbackTimer;
  uint32_t m_fallbackTimer;
  bool m_fallbackTimerLaunched = false;
  bool m_fallbackStarted;
  std::mutex m_mutexPendingFallbackBlock;
  std::shared_ptr<FallbackBlock> m_pendingFallbackBlock;
  std::mutex m_MutexCVFallbackBlock;
  std::condition_variable cv_fallbackBlock;
  std::mutex m_MutexCVFallbackConsensusObj;
  std::condition_variable cv_fallbackConsensusObj;
  bool m_runFallback;

  // Updating of ds guard var
  std::atomic_bool m_requestedForDSGuardNetworkInfoUpdate = {false};

  bool CheckState(Action action);

  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  // internal calls from ProcessStartPoW1
  bool ReadVariablesFromStartPoWMessage(const bytes& message,
                                        unsigned int cur_offset,
                                        uint64_t& block_num,
                                        uint8_t& ds_difficulty,
                                        uint8_t& difficulty,
                                        std::array<unsigned char, 32>& rand1,
                                        std::array<unsigned char, 32>& rand2);
  bool ProcessSubmitMissingTxn(const bytes& message, unsigned int offset,
                               const Peer& from);

  bool FindTxnInProcessedTxnsList(
      const uint64_t& blockNum, uint8_t sharing_mode,
      std::vector<TransactionWithReceipt>& txns_to_send,
      const TxnHash& tx_hash);

  bool LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                       const uint64_t& blocknum,
                                       bool& toSendTxnToLookup);

  bool ProcessStateDeltaFromFinalBlock(
      const bytes& stateDeltaBytes, const StateHash& finalBlockStateDeltaHash);

  // internal calls from ProcessForwardTransaction
  void CommitForwardedTransactions(const MBnForwardedTxnEntry& entry);

  bool RemoveTxRootHashFromUnavailableMicroBlock(
      const MBnForwardedTxnEntry& entry);

  bool IsMicroBlockTxRootHashInFinalBlock(const MBnForwardedTxnEntry& entry,
                                          bool& isEveryMicroBlockAvailable);

  // void StoreMicroBlocks();
  bool StoreFinalBlock(const TxBlock& txBlock);
  void InitiatePoW();
  void ScheduleMicroBlockConsensus();
  void BeginNextConsensusRound();

  void CommitMicroBlockConsensusBuffer();

  void DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
      const uint64_t& blocknum);

  void ReinstateMemPool(
      const std::map<Address, std::map<uint64_t, Transaction>>& addrNonceTxnMap,
      const std::vector<Transaction>& gasLimitExceededTxnBuffer);

  // internal calls from ProcessVCDSBlocksMessage
  void LogReceivedDSBlockDetails(const DSBlock& dsblock);
  void StoreDSBlockToDisk(const DSBlock& dsblock);

  // DS Guard network info update
  void QueryLookupForDSGuardNetworkInfoUpdate();

  // Message handlers
  bool ProcessStartPoW(const bytes& message, unsigned int offset,
                       const Peer& from);
  bool ProcessSharding(const bytes& message, unsigned int offset,
                       const Peer& from);
  bool ProcessSubmitTransaction(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessMicroBlockConsensus(const bytes& message, unsigned int offset,
                                  const Peer& from);
  bool ProcessMicroBlockConsensusCore(const bytes& message, unsigned int offset,
                                      const Peer& from);
  bool ProcessFinalBlock(const bytes& message, unsigned int offset,
                         const Peer& from);
  bool ProcessFinalBlockCore(const bytes& message, unsigned int offset,
                             const Peer& from, bool buffered = false);
  bool ProcessMBnForwardTransaction(const bytes& message,
                                    unsigned int cur_offset, const Peer& from);
  bool ProcessMBnForwardTransactionCore(const MBnForwardedTxnEntry& entry);
  bool ProcessTxnPacketFromLookup(const bytes& message, unsigned int offset,
                                  const Peer& from);
  bool ProcessTxnPacketFromLookupCore(const bytes& message,
                                      const uint64_t& epochNum,
                                      const uint64_t& dsBlockNum,
                                      const uint32_t& shardId,
                                      const PubKey& lookupPubKey,
                                      const std::vector<Transaction>& txns);
  bool ProcessProposeGasPrice(const bytes& message, unsigned int offset,
                              const Peer& from);

  bool ProcessDSGuardNetworkInfoUpdate(const bytes& message,
                                       unsigned int offset, const Peer& from);

  // bool ProcessCreateAccounts(const bytes & message,
  // unsigned int offset, const Peer & from);
  bool ProcessVCDSBlocksMessage(const bytes& message, unsigned int cur_offset,
                                const Peer& from);
  bool ProcessDoRejoin(const bytes& message, unsigned int offset,
                       const Peer& from);

  bool ComposeMBnForwardTxnMessageForSender(bytes& mb_txns_message);

  bool VerifyDSBlockCoSignature(const DSBlock& dsblock);
  bool VerifyFinalBlockCoSignature(const TxBlock& txblock);
  bool CheckStateRoot(const TxBlock& finalBlock);

  // View change

  bool VerifyVCBlockCoSignature(const VCBlock& vcblock);
  bool ProcessVCBlock(const bytes& message, unsigned int cur_offset,
                      const Peer& from);
  bool ProcessVCBlockCore(const VCBlock& vcblock);
  // Transaction functions
  bool OnCommitFailure(const std::map<unsigned int, bytes>&);

  bool RunConsensusOnMicroBlockWhenShardLeader();
  bool RunConsensusOnMicroBlockWhenShardBackup();
  bool ComposeMicroBlockMessageForSender(bytes& microblock_message) const;
  bool MicroBlockValidator(const bytes& message, unsigned int offset,
                           bytes& errorMsg, const uint32_t consensusID,
                           const uint64_t blockNumber, const bytes& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           bytes& messageToCosign);
  unsigned char CheckLegitimacyOfTxnHashes(bytes& errorMsg);
  bool CheckMicroBlockVersion();
  bool CheckMicroBlockshardId();
  bool CheckMicroBlockTimestamp();
  bool CheckMicroBlockHashes(bytes& errorMsg);
  bool CheckMicroBlockTxnRootHash();
  bool CheckMicroBlockStateDeltaHash();
  bool CheckMicroBlockTranReceiptHash();

  void NotifyTimeout(bool& txnProcTimeout);
  bool VerifyTxnsOrdering(const std::vector<TxnHash>& tranHashes,
                          std::vector<TxnHash>& missingtranHashes);

  // Fallback Consensus
  void FallbackTimerLaunch();
  void FallbackTimerPulse();
  void FallbackStop();
  bool FallbackValidator(const bytes& message, unsigned int offset,
                         bytes& errorMsg, const uint32_t consensusID,
                         const uint64_t blockNumber, const bytes& blockHash,
                         const uint16_t leaderID, const PubKey& leaderKey,
                         bytes& messageToCosign);
  void UpdateFallbackConsensusLeader();
  void SetLastKnownGoodState();
  bool ComposeFallbackBlock();
  void RunConsensusOnFallback();
  bool RunConsensusOnFallbackWhenLeader();
  bool RunConsensusOnFallbackWhenBackup();
  void ProcessFallbackConsensusWhenDone();
  bool ProcessFallbackConsensus(const bytes& message, unsigned int offset,
                                const Peer& from);
  // Fallback block processing
  bool VerifyFallbackBlockCoSignature(const FallbackBlock& fallbackblock);
  bool ProcessFallbackBlock(const bytes& message, unsigned int cur_offset,
                            const Peer& from);
  bool ComposeFallbackBlockMessageForSender(bytes& fallbackblock_message) const;

  // Is Running from New Process
  bool m_fromNewProcess = true;

  bool m_doRejoinAtNextRound = false;
  bool m_doRejoinAtStateRoot = false;
  bool m_doRejoinAtFinalBlock = false;

  void ResetRejoinFlags();

  void SendDSBlockToOtherShardNodes(const bytes& dsblock_message);
  void SendVCBlockToOtherShardNodes(const bytes& vcblock_message);
  void SendFallbackBlockToOtherShardNodes(const bytes& fallbackblock_message);
  void SendBlockToOtherShardNodes(const bytes& message, uint32_t cluster_size,
                                  uint32_t num_of_child_clusters);
  void GetNodesToBroadCastUsingTreeBasedClustering(
      uint32_t cluster_size, uint32_t num_of_child_clusters, uint32_t& nodes_lo,
      uint32_t& nodes_hi);

  void GetIpMapping(std::unordered_map<std::string, Peer>& ipMapping);

  void WakeupAtDSEpoch();

  void WakeupAtTxEpoch();

  /// Set initial state, variables, and clean-up storage
  void Init();

  /// Initilize the add genesis block and account
  void AddGenesisInfo(SyncType syncType);

 public:
  enum NodeState : unsigned char {
    POW_SUBMISSION = 0x00,
    WAITING_DSBLOCK,
    MICROBLOCK_CONSENSUS_PREP,
    MICROBLOCK_CONSENSUS,
    WAITING_FINALBLOCK,
    FALLBACK_CONSENSUS_PREP,
    FALLBACK_CONSENSUS,
    WAITING_FALLBACKBLOCK,
    SYNC
  };

  // Proposed gas price
  uint128_t m_proposedGasPrice;
  std::mutex m_mutexGasPrice;

  // This process is newly invoked by shell from late node join script
  bool m_runFromLate = false;

  // std::condition_variable m_cvAllMicroBlocksRecvd;
  // std::mutex m_mutexAllMicroBlocksRecvd;
  // bool m_allMicroBlocksRecvd = true;

  std::mutex m_mutexShardMember;
  std::shared_ptr<DequeOfNode> m_myShardMembers;

  std::shared_ptr<MicroBlock> m_microblock;

  std::mutex m_mutexCVMicroBlockMissingTxn;
  std::condition_variable cv_MicroBlockMissingTxn;

  // std::condition_variable m_cvNewRoundStarted;
  // std::mutex m_mutexNewRoundStarted;
  // bool m_newRoundStarted = false;

  std::mutex m_mutexIsEveryMicroBlockAvailable;

  // Transaction body sharing variables
  std::mutex m_mutexUnavailableMicroBlocks;
  std::unordered_map<uint64_t, std::vector<std::pair<BlockHash, TxnHash>>>
      m_unavailableMicroBlocks;

  /// Sharding variables
  std::atomic<uint32_t> m_myshardId;
  std::atomic<bool> m_isPrimary;
  std::shared_ptr<ConsensusCommon> m_consensusObject;

  // Finalblock Processing
  std::mutex m_mutexFinalBlock;

  // DS block information
  std::mutex m_mutexDSBlock;

  /// The current internal state of this Node instance.
  std::atomic<NodeState> m_state;

  // a buffer flag used by lookup to store the isVacuousEpoch state before
  // StoreFinalBlock
  std::atomic<bool> m_isVacuousEpochBuffer;

  // an indicator that whether the non-sync node is still doing mining
  // at standard difficulty
  std::atomic<bool> m_stillMiningPrimary;

  // a indicator of whether recovered from fallback just now
  bool m_justDidFallback = false;

  /// Constructor. Requires mediator reference to access DirectoryService and
  /// other global members.
  Node(Mediator& mediator, unsigned int syncType, bool toRetrieveHistory);

  /// Destructor.
  ~Node();

  /// Install the Node
  bool Install(const SyncType syncType, const bool toRetrieveHistory = true,
               bool rejoiningAfterRecover = false);

  // Reset certain variables to the initial state
  bool CleanVariables();

  /// Prepare for processing protocols after initialization
  void Prepare(bool runInitializeGenesisBlocks);

  /// Get number of shards
  uint32_t getNumShards() { return m_numShards; };

  /// Get this node shard ID
  uint32_t GetShardId() { return m_myshardId; };

  /// Sets the value of m_state.
  void SetState(NodeState state);

  /// Implements the Execute function inherited from Executable.
  bool Execute(const bytes& message, unsigned int offset, const Peer& from);

  Mediator& GetMediator() { return m_mediator; }

  /// Download peristence from incremental db
  bool DownloadPersistenceFromS3();

  /// Recover the previous state by retrieving persistence data
  bool StartRetrieveHistory(const SyncType syncType,
                            bool rejoiningAfterRecover = false);

  bool CheckIntegrity();

  bool ValidateDB();

  // Erase m_committedTransactions for given epoch number
  // void EraseCommittedTransactions(uint64_t epochNum)
  // {
  //     std::lock_guard<std::mutex> g(m_mutexCommittedTransactions);
  //     m_committedTransactions.erase(epochNum);
  // }

  /// Add new block into tx blockchain
  void AddBlock(const TxBlock& block);

  void UpdateDSCommiteeComposition(DequeOfNode& dsComm, const DSBlock& dsblock);

  void UpdateDSCommitteeAfterFallback(const uint32_t& shard_id,
                                      const PubKey& leaderPubKey,
                                      const Peer& leaderNetworkInfo,
                                      DequeOfNode& dsComm,
                                      const DequeOfShard& shards);

  void CommitMBnForwardedTransactionBuffer();

  void CleanCreatedTransaction();

  void AddBalanceToGenesisAccount();

  void PopulateAccounts();

  void UpdateBalanceForPreGeneratedAccounts();

  void AddToMicroBlockConsensusBuffer(uint32_t consensusId,
                                      const bytes& message, unsigned int offset,
                                      const Peer& peer,
                                      const PubKey& senderPubKey);
  void CleanMicroblockConsensusBuffer();

  void CallActOnFinalblock();

  void ProcessTransactionWhenShardLeader();
  void ProcessTransactionWhenShardBackup();
  bool ComposeMicroBlock();
  bool CheckMicroBlockValidity(bytes& errorMsg);
  bool OnNodeMissingTxns(const bytes& errorMsg, const unsigned int offset,
                         const Peer& from);

  void UpdateStateForNextConsensusRound();

  // Start synchronization with lookup as a shard node
  void StartSynchronization();

  /// Performs PoW mining and submission for DirectoryService committee
  /// membership.
  bool StartPoW(const uint64_t& block_num, uint8_t ds_difficulty,
                uint8_t difficulty,
                const std::array<unsigned char, UINT256_SIZE>& rand1,
                const std::array<unsigned char, UINT256_SIZE>& rand2,
                const uint32_t lookupId = uint32_t() - 1);

  /// Send PoW soln to DS Commitee
  bool SendPoWResultToDSComm(const uint64_t& block_num,
                             const uint8_t& difficultyLevel,
                             const uint64_t winningNonce,
                             const std::string& powResultHash,
                             const std::string& powMixhash,
                             const uint32_t& lookupId,
                             const uint128_t& gasPrice);

  /// Used by oldest DS node to configure shard ID as a new shard node
  void SetMyshardId(uint32_t shardId);

  /// Used by oldest DS node to finish setup as a new shard node
  void StartFirstTxEpoch();

  /// Used for start consensus on microblock
  bool RunConsensusOnMicroBlock();

  /// Used for commit buffered txn packet
  void CommitTxnPacketBuffer();

  /// Used by oldest DS node to configure sharding variables as a new shard node
  bool LoadShardingStructure(bool callByRetrieve = false);

  // Rejoin the network as a shard node in case of failure happens in protocol
  void RejoinAsNormal();

  /// Force state changes from MBCON/MBCON_PREP -> WAITING_FINALBLOCK
  void PrepareGoodStateForFinalBlock();

  /// Reset Consensus ID
  void ResetConsensusId();

  // Set m_consensusMyID
  void SetConsensusMyID(uint16_t);

  // Get m_consensusMyID
  uint16_t GetConsensusMyID() const;

  // Set m_consensusLeaderID
  void SetConsensusLeaderID(uint16_t);

  // Get m_consensusLeaderID
  uint16_t GetConsensusLeaderID() const;

  /// Fetch offline lookups with a counter for retrying
  bool GetOfflineLookups(bool endless = false);

  /// Fetch latest ds block with a counter for retrying
  bool GetLatestDSBlock();

  void UpdateDSCommiteeCompositionAfterVC(const VCBlock& vcblock,
                                          DequeOfNode& dsComm);
  void UpdateRetrieveDSCommiteeCompositionAfterVC(const VCBlock& vcblock,
                                                  DequeOfNode& dsComm);

  void UpdateProcessedTransactions();

  bool IsShardNode(const PubKey& pubKey);
  bool IsShardNode(const Peer& peerInfo);

  PoolTxnStatus IsTxnInMemPool(const TxnHash& txhash) const;

  uint32_t CalculateShardLeaderFromDequeOfNode(uint16_t lastBlockHash,
                                               uint32_t sizeOfShard,
                                               const DequeOfNode& shardMembers);
  uint32_t CalculateShardLeaderFromShard(uint16_t lastBlockHash,
                                         uint32_t sizeOfShard,
                                         const Shard& shardMembers);

  static bool GetDSLeader(const BlockLink& lastBlockLink,
                          const DSBlock& latestDSBlock,
                          const DequeOfNode& dsCommittee, PairOfNode& dsLeader);

  // Get entire network peer info
  void GetEntireNetworkPeerInfo(VectorOfNode& peers,
                                std::vector<PubKey>& pubKeys);

  std::string GetStateString() const;

 private:
  static std::map<NodeState, std::string> NodeStateStrings;

  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
  /// Fallback Consensus Related
  std::atomic<NodeState> m_fallbackState;
  bool ValidateFallbackState(NodeState nodeState, NodeState statePropose);
};

#endif  // __NODE_H__
