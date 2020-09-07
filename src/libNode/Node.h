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
#ifndef ZILLIQA_SRC_LIBNODE_NODE_H_
#define ZILLIQA_SRC_LIBNODE_NODE_H_

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
#include "common/TxnStatus.h"
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

typedef std::unordered_map<uint64_t, std::vector<std::pair<BlockHash, TxnHash>>>
    UnavailableMicroBlockList;

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

  struct GovProposalInfo {
    GovProposalIdVotePair proposal;
    uint64_t startDSEpoch;
    uint64_t endDSEpoch;
    int32_t remainingVoteCount;
    bool isGovProposalActive{false};
    GovProposalInfo()
        : proposal({0, 0}),
          startDSEpoch(0),
          endDSEpoch(0),
          remainingVoteCount(0) {}
    void reset() {
      proposal = std::make_pair(0, 0);
      isGovProposalActive = false;
      startDSEpoch = endDSEpoch = remainingVoteCount = 0;
    }
  };

  Mediator& m_mediator;

  Synchronizer m_synchronizer;

  // DS block information
  std::mutex m_mutexConsensus;

  // Sharding information
  std::atomic<uint32_t> m_numShards{};

  // pre-generated addresses
  std::vector<Address> m_populatedAddresses;
  unsigned int m_accountPopulated = 0;

  // Consensus variables
  std::mutex m_mutexProcessConsensusMessage;
  std::condition_variable cv_processConsensusMessage;
  std::mutex m_MutexCVMicroblockConsensus;
  std::mutex m_MutexCVMicroblockConsensusObject;
  std::condition_variable cv_microblockConsensusObject;
  std::atomic<uint16_t> m_consensusMyID{};
  std::atomic<uint16_t> m_consensusLeaderID{};

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
  std::mutex m_mutexCreatedTransactions;
  TxnPool m_createdTxns, t_createdTxns;

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
  std::shared_timed_mutex mutable m_unconfirmedTxnsMutex;
  HashCodeMap m_unconfirmedTxns;

  std::shared_timed_mutex mutable m_droppedTxnsMutex;
  TTLTxns m_droppedTxns;

  std::shared_timed_mutex mutable m_pendingTxnsMutex;
  TTLTxns m_pendingTxns;

  std::mutex m_mutexMBnForwardedTxnBuffer;
  std::unordered_map<uint64_t, std::vector<MBnForwardedTxnEntry>>
      m_mbnForwardedTxnBuffer;

  std::mutex m_mutexPendingTxnBuffer;
  std::unordered_map<uint64_t,
                     std::vector<std::tuple<HashCodeMap, PubKey, uint32_t>>>
      m_pendingTxnBuffer;

  std::mutex m_mutexTxnPacketBuffer;
  std::map<bytes, bytes> m_txnPacketBuffer;

  // txn proc timeout related
  std::mutex m_mutexCVTxnProcFinished;
  std::condition_variable cv_TxnProcFinished;

  std::mutex m_mutexMicroBlockConsensusBuffer;
  std::unordered_map<uint32_t, VectorOfNodeMsg> m_microBlockConsensusBuffer;

  // soft confirmed transactions
  std::mutex m_mutexSoftConfirmedTxns;
  std::unordered_map<TxnHash, TransactionWithReceipt> m_softConfirmedTxns;

  // Fallback Consensus
  std::mutex m_mutexFallbackTimer;
  uint32_t m_fallbackTimer{};
  bool m_fallbackTimerLaunched = false;
  bool m_fallbackStarted{};
  std::mutex m_mutexPendingFallbackBlock;
  std::shared_ptr<FallbackBlock> m_pendingFallbackBlock;
  std::mutex m_MutexCVFallbackBlock;
  std::condition_variable cv_fallbackBlock;
  std::mutex m_MutexCVFallbackConsensusObj;
  std::condition_variable cv_fallbackConsensusObj;
  bool m_runFallback{};
  // pair of proposal id and vote value and vote duration in epoch
  std::mutex m_mutexGovProposal;
  GovProposalInfo m_govProposalInfo;

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

  bool ProcessStateDeltaFromFinalBlock(
      const bytes& stateDeltaBytes, const StateHash& finalBlockStateDeltaHash);

  // internal calls from ProcessForwardTransaction
  void CommitForwardedTransactions(const MBnForwardedTxnEntry& entry);

  bool AddPendingTxn(const HashCodeMap& pendingTxns, const PubKey& pubkey,
                     uint32_t shardId);

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
      const std::vector<Transaction>& gasLimitExceededTxnBuffer,
      const std::vector<std::pair<TxnHash, TxnStatus>>& droppedTxns);

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
  bool ProcessVCFinalBlock(const bytes& message, unsigned int offset,
                           const Peer& from);
  bool ProcessVCFinalBlockCore(const bytes& message, unsigned int offset,
                               const Peer& from);
  bool ProcessFinalBlock(const bytes& message, unsigned int offset,
                         const Peer& from);
  bool ProcessFinalBlockCore(uint64_t& dsBlockNumber, uint32_t& consensusID,
                             TxBlock& txBlock, bytes& stateDelta,
                             const uint64_t& messageSize);
  bool ProcessMBnForwardTransaction(const bytes& message,
                                    unsigned int cur_offset, const Peer& from);
  bool ProcessMBnForwardTransactionCore(const MBnForwardedTxnEntry& entry);

  bool ProcessPendingTxn(const bytes& message, unsigned int cur_offset,
                         const Peer& from);
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

  bool ProcessNewShardNodeNetworkInfo(const bytes& message, unsigned int offset,
                                      const Peer& from);

  // bool ProcessCreateAccounts(const bytes & message,
  // unsigned int offset, const Peer & from);
  bool ProcessVCDSBlocksMessage(const bytes& message, unsigned int cur_offset,
                                const Peer& from);
  bool ProcessDoRejoin(const bytes& message, unsigned int offset,
                       const Peer& from);

  bool ProcessRemoveNodeFromBlacklist(const bytes& message, unsigned int offset,
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
  bool CheckMicroBlockGasLimit(const uint64_t& microblock_gas_limit);
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

  void SoftConfirmForwardedTransactions(const MBnForwardedTxnEntry& entry);
  void ClearSoftConfirmedTransactions();
  void UpdateGovProposalRemainingVoteInfo();
  bool CheckIfGovProposalActive();

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

  enum RECEIVERTYPE : unsigned char { LOOKUP = 0x00, PEER, BOTH };

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
  UnavailableMicroBlockList m_unavailableMicroBlocks;

  /// Sharding variables
  std::atomic<uint32_t> m_myshardId{};
  std::atomic<bool> m_isPrimary{};
  std::shared_ptr<ConsensusCommon> m_consensusObject;

  // Finalblock Processing
  std::mutex m_mutexFinalBlock;

  // VCFinalblock Processing
  std::mutex m_mutexVCFinalBlock;

  // DS block information
  std::mutex m_mutexDSBlock;

  // VC block information
  std::mutex m_mutexVCBlock;

  /// The current internal state of this Node instance.
  std::atomic<NodeState> m_state{};

  // a buffer flag used by lookup to store the isVacuousEpoch state before
  // StoreFinalBlock
  std::atomic<bool> m_isVacuousEpochBuffer{};

  // an indicator that whether the non-sync node is still doing mining
  // at standard difficulty
  std::atomic<bool> m_stillMiningPrimary{};

  // a indicator of whether recovered from fallback just now
  bool m_justDidFallback = false;

  // Is part of current sharding structure / dsCommittee
  std::atomic<bool> m_confirmedNotInNetwork{};

  // hold count of whitelist request for given ip
  std::mutex m_mutexWhitelistReqs;
  std::map<uint128_t, uint32_t> m_whitelistReqs;

  // store VCBlocks if any of latest tx block
  std::mutex m_mutexvcBlocksStore;
  std::vector<VCBlock> m_vcBlockStore;

  // store VCDSBlocks
  std::mutex m_mutexVCDSBlockStore;
  std::map<uint64_t, bytes> m_vcDSBlockStore;

  // store VCFinalBlocks
  std::mutex m_mutexVCFinalBlockStore;
  std::map<uint64_t, bytes> m_vcFinalBlockStore;

  // store MBNFORWARDTRANSACTION
  std::mutex m_mutexMBnForwardedTxnStore;
  std::map<uint64_t, std::map<uint32_t, bytes>> m_mbnForwardedTxnStore;

  // store PENDINGTXN
  std::mutex m_mutexPendingTxnStore;
  std::map<uint64_t, std::map<uint32_t, bytes>> m_pendingTxnStore;

  // stores historical map of vcblocks to txblocknum
  std::mutex m_mutexhistVCBlkForTxBlock;
  std::map<uint64_t, std::vector<VCBlockSharedPtr>> m_histVCBlocksForTxBlock;

  // stores historical map of vcblocks to dsblocknum
  std::mutex m_mutexhistVCBlkForDSBlock;
  std::map<uint64_t, std::vector<VCBlockSharedPtr>> m_histVCBlocksForDSBlock;

  // whether txns dist window open
  std::atomic<bool> m_txn_distribute_window_open{};

  // stores map of shardnodepubkey and n/w info change request count (for
  // current dsepoch only)
  std::mutex m_mutexIPChangeRequestStore;
  std::map<PubKey, uint32_t> m_ipChangeRequestStore;

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

  /// Recalculate this node shardID and if IP was changed
  bool RecalculateMyShardId(bool& ipChanged);

  // Send whitelist message to peers and seeds
  bool ComposeAndSendRemoveNodeFromBlacklist(
      const RECEIVERTYPE receiver = BOTH);

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

  bool CheckIntegrity(const bool fromValidateDBBinary = false);
  void PutProcessedInUnconfirmedTxns();

  bool SendPendingTxnToLookup();

  bool ValidateDB();

  // Erase m_committedTransactions for given epoch number
  // void EraseCommittedTransactions(uint64_t epochNum)
  // {
  //     std::lock_guard<std::mutex> g(m_mutexCommittedTransactions);
  //     m_committedTransactions.erase(epochNum);
  // }

  /// Add new block into tx blockchain
  void AddBlock(const TxBlock& block);

  void UpdateDSCommitteeComposition(DequeOfNode& dsComm, const DSBlock& dsblock,
                                    const bool showLogs = true);
  void UpdateDSCommitteeComposition(DequeOfNode& dsComm, const DSBlock& dsblock,
                                    MinerInfoDSComm& minerInfo);

  void UpdateDSCommitteeAfterFallback(const uint32_t& shard_id,
                                      const PubKey& leaderPubKey,
                                      const Peer& leaderNetworkInfo,
                                      DequeOfNode& dsComm,
                                      const DequeOfShard& shards);

  void CommitMBnForwardedTransactionBuffer();

  void CleanCreatedTransaction();

  void AddBalanceToGenesisAccount();

  void PopulateAccounts(bool temp = false);

  void UpdateBalanceForPreGeneratedAccounts();

  void AddToMicroBlockConsensusBuffer(uint32_t consensusId,
                                      const bytes& message, unsigned int offset,
                                      const Peer& peer,
                                      const PubKey& senderPubKey);
  void CleanMicroblockConsensusBuffer();

  void CallActOnFinalblock();

  void CommitPendingTxnBuffer();

  void ProcessTransactionWhenShardLeader(const uint64_t& microblock_gas_limit);
  void ProcessTransactionWhenShardBackup(const uint64_t& microblock_gas_limit);
  bool ComposeMicroBlock(const uint64_t& microblock_gas_limit);
  bool CheckMicroBlockValidity(bytes& errorMsg,
                               const uint64_t& microblock_gas_limit);

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

  /// Send PoW soln to DS Committee
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
  /// And also used by shard node rejoining back
  void StartFirstTxEpoch(bool fbWaitState = false);

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

  void UpdateDSCommitteeCompositionAfterVC(const VCBlock& vcblock,
                                           DequeOfNode& dsComm);
  void UpdateRetrieveDSCommitteeCompositionAfterVC(const VCBlock& vcblock,
                                                   DequeOfNode& dsComm,
                                                   const bool showLogs = true);

  void UpdateProcessedTransactions();

  bool IsShardNode(const PubKey& pubKey);
  bool IsShardNode(const Peer& peerInfo);

  TxnStatus IsTxnInMemPool(const TxnHash& txhash) const;

  std::unordered_map<TxnHash, TxnStatus> GetUnconfirmedTxns() const;

  std::unordered_map<TxnHash, TxnStatus> GetDroppedTxns() const;

  std::unordered_map<TxnHash, TxnStatus> GetPendingTxns() const;

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

  bool LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                       bool& toSendTxnToLookup,
                                       bool skipShardIDCheck = false);

  UnavailableMicroBlockList& GetUnavailableMicroBlocks();

  void CleanUnavailableMicroBlocks();

  bool WhitelistReqsValidator(const uint128_t& ipAddress);

  void CleanWhitelistReqs();

  void ClearUnconfirmedTxn();

  void ClearPendingAndDroppedTxn();

  void ClearAllPendingAndDroppedTxn();

  bool IsUnconfirmedTxnEmpty() const;

  void RemoveIpMapping();

  void CleanLocalRawStores();

  bool GetSoftConfirmedTransaction(const TxnHash& txnHash,
                                   TxBodySharedPtr& tptr);
  void WaitForNextTwoBlocksBeforeRejoin();

  bool UpdateShardNodeIdentity();

  bool ValidateAndUpdateIPChangeRequestStore(const PubKey& shardNodePubkey);

  bool StoreVoteUntilPow(const std::string& proposalId,
                         const std::string& voteValue,
                         const std::string& remainingVoteCount,
                         const std::string& startDSEpoch,
                         const std::string& endDSEpoch);

 private:
  static std::map<NodeState, std::string> NodeStateStrings;

  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
  /// Fallback Consensus Related
  std::atomic<NodeState> m_fallbackState{};
  bool ValidateFallbackState(NodeState nodeState, NodeState statePropose);

  void PutTxnsInTempDataBase(
      const std::unordered_map<TxnHash, TransactionWithReceipt>&
          processedTransactions);

  void SaveTxnsToS3(const std::unordered_map<TxnHash, TransactionWithReceipt>&
                        processedTransactions);

  std::string GetAwsS3CpString(const std::string& uploadFilePath);
};

#endif  // ZILLIQA_SRC_LIBNODE_NODE_H_
