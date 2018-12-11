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

#include "common/Broadcastable.h"
#include "common/Constants.h"
#include "common/Executable.h"
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
#include "libNetwork/PeerStore.h"
#include "libPersistence/BlockStorage.h"

class Mediator;
class Retriever;

/// Implements PoW submission and sharding node functionality.
class Node : public Executable, public Broadcastable {
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
    ATSTATEROOT = 0x02
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

  // Consensus variables
  std::mutex m_mutexProcessConsensusMessage;
  std::condition_variable cv_processConsensusMessage;
  std::mutex m_MutexCVMicroblockConsensus;
  std::mutex m_MutexCVMicroblockConsensusObject;
  std::condition_variable cv_microblockConsensusObject;

  std::mutex m_MutexCVFBWaitMB;
  std::condition_variable cv_FBWaitMB;

  /// DSBlock Timer Vars
  std::mutex m_mutexCVWaitDSBlock;
  std::condition_variable cv_waitDSBlock;

  // Persistence Retriever
  std::shared_ptr<Retriever> m_retriever;

  std::vector<unsigned char> m_consensusBlockHash;
  std::pair<uint64_t, CoSignatures> m_lastMicroBlockCoSig;
  std::mutex m_mutexMicroBlock;

  const static uint32_t RECVTXNDELAY_MILLISECONDS = 3000;
  const static unsigned int GOSSIP_RATE = 48;

  // Transactions information
  std::mutex m_mutexCreatedTransactions;
  TxnPool m_createdTxns, t_createdTxns;
  std::vector<TxnHash> m_txnsOrdering;
  std::mutex m_mutexProcessedTransactions;
  std::unordered_map<uint64_t,
                     std::unordered_map<TxnHash, TransactionWithReceipt>>
      m_processedTransactions;
  std::unordered_map<TxnHash, TransactionWithReceipt> t_processedTransactions;
  // operates under m_mutexProcessedTransaction
  std::vector<TxnHash> m_TxnOrder;

  uint32_t m_numOfAbsentTxnHashes;

  uint64_t m_gasUsedTotal;
  boost::multiprecision::uint128_t m_txnFees;

  // std::mutex m_mutexCommittedTransactions;
  // std::unordered_map<uint64_t, std::list<TransactionWithReceipt>>
  //     m_committedTransactions;

  std::mutex m_mutexMBnForwardedTxnBuffer;
  std::unordered_map<uint64_t, std::vector<MBnForwardedTxnEntry>>
      m_mbnForwardedTxnBuffer;

  std::mutex m_mutexTxnPacketBuffer;
  std::vector<std::vector<unsigned char>> m_txnPacketBuffer;

  std::mutex m_mutexMicroBlockConsensusBuffer;
  std::unordered_map<uint32_t,
                     std::vector<std::pair<Peer, std::vector<unsigned char>>>>
      m_microBlockConsensusBuffer;

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

  bool CheckState(Action action);

  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  // internal calls from ProcessStartPoW1
  bool ReadVariablesFromStartPoWMessage(
      const std::vector<unsigned char>& message, unsigned int cur_offset,
      uint64_t& block_num, uint8_t& ds_difficulty, uint8_t& difficulty,
      std::array<unsigned char, 32>& rand1,
      std::array<unsigned char, 32>& rand2);
  bool ProcessSubmitMissingTxn(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);

  bool FindTxnInProcessedTxnsList(
      const uint64_t& blockNum, uint8_t sharing_mode,
      std::vector<TransactionWithReceipt>& txns_to_send,
      const TxnHash& tx_hash);

  bool LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                       const uint64_t& blocknum,
                                       bool& toSendTxnToLookup);

  bool ProcessStateDeltaFromFinalBlock(
      const std::vector<unsigned char>& stateDeltaBytes,
      const StateHash& finalBlockStateDeltaHash);

  // internal calls from ProcessForwardTransaction
  void CommitForwardedTransactions(const MBnForwardedTxnEntry& entry);

  bool RemoveTxRootHashFromUnavailableMicroBlock(
      const MBnForwardedTxnEntry& entry);

  bool IsMicroBlockTxRootHashInFinalBlock(const MBnForwardedTxnEntry& entry,
                                          bool& isEveryMicroBlockAvailable);

  void StoreState();
  // void StoreMicroBlocks();
  void StoreFinalBlock(const TxBlock& txBlock);
  void InitiatePoW();
  void ScheduleMicroBlockConsensus();
  void BeginNextConsensusRound();

  void CommitMicroBlockConsensusBuffer();

  void DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
      const uint64_t& blocknum);
  void LogReceivedFinalBlockDetails(const TxBlock& txblock);

  // internal calls from ProcessVCDSBlocksMessage
  void LogReceivedDSBlockDetails(const DSBlock& dsblock);
  void StoreDSBlockToDisk(const DSBlock& dsblock);

  // Message handlers
  bool ProcessStartPoW(const std::vector<unsigned char>& message,
                       unsigned int offset, const Peer& from);
  bool ProcessSharding(const std::vector<unsigned char>& message,
                       unsigned int offset, const Peer& from);
  bool ProcessSubmitTransaction(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  bool ProcessMicroblockConsensus(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
  bool ProcessMicroblockConsensusCore(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from);
  bool ProcessFinalBlock(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);
  bool ProcessMBnForwardTransaction(const std::vector<unsigned char>& message,
                                    unsigned int cur_offset, const Peer& from);
  bool ProcessMBnForwardTransactionCore(const MBnForwardedTxnEntry& entry);
  bool ProcessTxnPacketFromLookup(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
  bool ProcessTxnPacketFromLookupCore(const std::vector<unsigned char>& message,
                                      const uint64_t& dsBlockNum,
                                      const uint32_t& shardId,
                                      const std::vector<Transaction>& txns);
  bool ProcessProposeGasPrice(const std::vector<unsigned char>& message,
                              unsigned int offset, const Peer& from);

#ifdef HEARTBEAT_TEST
  bool ProcessKillPulse(const std::vector<unsigned char>& message,
                        unsigned int offset, const Peer& from);
#endif  // HEARTBEAT_TEST

  // bool ProcessCreateAccounts(const std::vector<unsigned char> & message,
  // unsigned int offset, const Peer & from);
  bool ProcessVCDSBlocksMessage(const std::vector<unsigned char>& message,
                                unsigned int cur_offset, const Peer& from);
  bool ProcessDoRejoin(const std::vector<unsigned char>& message,
                       unsigned int offset, const Peer& from);

  bool ComposeMBnForwardTxnMessageForSender(
      std::vector<unsigned char>& mb_txns_message);

  bool VerifyDSBlockCoSignature(const DSBlock& dsblock);
  bool VerifyFinalBlockCoSignature(const TxBlock& txblock);
  bool CheckStateRoot(const TxBlock& finalBlock);

  // View change

  bool VerifyVCBlockCoSignature(const VCBlock& vcblock);
  bool ProcessVCBlock(const std::vector<unsigned char>& message,
                      unsigned int cur_offset, const Peer& from);
  bool ProcessVCBlockCore(const VCBlock& vcblock);
  // Transaction functions
  bool OnCommitFailure(
      const std::map<unsigned int, std::vector<unsigned char>>&);

  bool RunConsensusOnMicroBlockWhenShardLeader();
  bool RunConsensusOnMicroBlockWhenShardBackup();
  bool ComposeMicroBlockMessageForSender(
      std::vector<unsigned char>& microblock_message) const;
  bool MicroBlockValidator(const std::vector<unsigned char>& message,
                           unsigned int offset,
                           std::vector<unsigned char>& errorMsg,
                           const uint32_t consensusID,
                           const uint64_t blockNumber,
                           const std::vector<unsigned char>& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           std::vector<unsigned char>& messageToCosign);
  unsigned char CheckLegitimacyOfTxnHashes(
      std::vector<unsigned char>& errorMsg);
  bool CheckBlockTypeIsMicro();
  bool CheckMicroBlockVersion();
  bool CheckMicroBlockshardId();
  bool CheckMicroBlockTimestamp();
  bool CheckMicroBlockHashes(std::vector<unsigned char>& errorMsg);
  bool CheckMicroBlockTxnRootHash();
  bool CheckMicroBlockStateDeltaHash();
  bool CheckMicroBlockTranReceiptHash();

  bool VerifyTxnsOrdering(const std::vector<TxnHash>& tranHashes);

  // Fallback Consensus
  void FallbackTimerLaunch();
  void FallbackTimerPulse();
  void FallbackStop();
  bool FallbackValidator(const std::vector<unsigned char>& message,
                         unsigned int offset,
                         std::vector<unsigned char>& errorMsg,
                         const uint32_t consensusID, const uint64_t blockNumber,
                         const std::vector<unsigned char>& blockHash,
                         const uint16_t leaderID, const PubKey& leaderKey,
                         std::vector<unsigned char>& messageToCosign);
  void UpdateFallbackConsensusLeader();
  void SetLastKnownGoodState();
  bool ComposeFallbackBlock();
  void RunConsensusOnFallback();
  bool RunConsensusOnFallbackWhenLeader();
  bool RunConsensusOnFallbackWhenBackup();
  void ProcessFallbackConsensusWhenDone();
  bool ProcessFallbackConsensus(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  // Fallback block processing
  bool VerifyFallbackBlockCoSignature(const FallbackBlock& fallbackblock);
  bool ProcessFallbackBlock(const std::vector<unsigned char>& message,
                            unsigned int cur_offset, const Peer& from);
  bool ComposeFallbackBlockMessageForSender(
      std::vector<unsigned char>& fallbackblock_message) const;

  // Is Running from New Process
  bool m_fromNewProcess = true;

  bool m_doRejoinAtNextRound = false;
  bool m_doRejoinAtStateRoot = false;
  bool m_doRejoinAtFinalBlock = false;

  void ResetRejoinFlags();

  void SendDSBlockToOtherShardNodes(
      const std::vector<unsigned char>& dsblock_message);
  void SendVCBlockToOtherShardNodes(
      const std::vector<unsigned char>& vcblock_message);
  void SendFallbackBlockToOtherShardNodes(
      const std::vector<unsigned char>& fallbackblock_message);
  void SendBlockToOtherShardNodes(const std::vector<unsigned char>& message,
                                  uint32_t cluster_size,
                                  uint32_t num_of_child_clusters);
  void GetNodesToBroadCastUsingTreeBasedClustering(
      uint32_t cluster_size, uint32_t num_of_child_clusters, uint32_t& nodes_lo,
      uint32_t& nodes_hi);

  void GetIpMapping(std::unordered_map<std::string, Peer>& ipMapping);

  void WakeupForUpgrade();

  void WakeupForRecovery();

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
  boost::multiprecision::uint128_t m_proposedGasPrice;
  std::mutex m_mutexGasPrice;

  // This process is newly invoked by shell from late node join script
  bool m_runFromLate = false;

  // std::condition_variable m_cvAllMicroBlocksRecvd;
  // std::mutex m_mutexAllMicroBlocksRecvd;
  // bool m_allMicroBlocksRecvd = true;

  std::mutex m_mutexShardMember;
  std::shared_ptr<std::deque<std::pair<PubKey, Peer>>> m_myShardMembers;

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
  std::atomic<uint32_t> m_consensusMyID;
  std::atomic<bool> m_isPrimary;
  std::atomic<uint32_t> m_consensusLeaderID;
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
  bool Install(const SyncType syncType, const bool toRetrieveHistory = true);

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
  bool Execute(const std::vector<unsigned char>& message, unsigned int offset,
               const Peer& from);

  /// Implements the GetBroadcastList function inherited from Broadcastable.
  std::vector<Peer> GetBroadcastList(unsigned char ins_type,
                                     const Peer& broadcast_originator);

  Mediator& GetMediator() { return m_mediator; }

  /// Recover the previous state by retrieving persistence data
  bool StartRetrieveHistory(const SyncType syncType, bool& wakeupForUpgrade);

  // Erase m_committedTransactions for given epoch number
  // void EraseCommittedTransactions(uint64_t epochNum)
  // {
  //     std::lock_guard<std::mutex> g(m_mutexCommittedTransactions);
  //     m_committedTransactions.erase(epochNum);
  // }

  /// Add new block into tx blockchain
  void AddBlock(const TxBlock& block);

  void UpdateDSCommiteeComposition(std::deque<std::pair<PubKey, Peer>>& dsComm,
                                   const DSBlock& dsblock);

  void UpdateDSCommitteeAfterFallback(
      const uint32_t& shard_id, const PubKey& leaderPubKey,
      const Peer& leaderNetworkInfo,
      std::deque<std::pair<PubKey, Peer>>& dsComm, const DequeOfShard& shards);

  void CommitMBnForwardedTransactionBuffer();

  void CleanCreatedTransaction();

  void CleanMicroblockConsensusBuffer();

  void CallActOnFinalblock();

  void ProcessTransactionWhenShardLeader();
  bool ProcessTransactionWhenShardBackup(
      const std::vector<TxnHash>& tranHashes,
      std::vector<TxnHash>& missingtranHashes);
  bool ComposeMicroBlock();
  bool CheckMicroBlockValidity(std::vector<unsigned char>& errorMsg);
  bool OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
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
                             const boost::multiprecision::uint128_t& gasPrice);

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

  /// Fetch offline lookups with a counter for retrying
  bool GetOfflineLookups(bool endless = false);

  /// Fetch latest ds block with a counter for retrying
  bool GetLatestDSBlock();

  void UpdateDSCommiteeCompositionAfterVC(
      const VCBlock& vcblock, std::deque<std::pair<PubKey, Peer>>& dsComm);
  void UpdateRetrieveDSCommiteeCompositionAfterVC(
      const VCBlock& vcblock, std::deque<std::pair<PubKey, Peer>>& dsComm);

  void UpdateProcessedTransactions();

  bool IsShardNode(const PubKey& pubKey);
  bool IsShardNode(const Peer& peerInfo);

 private:
  static std::map<NodeState, std::string> NodeStateStrings;
  std::string GetStateString() const;
  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
  /// Fallback Consensus Related
  std::atomic<NodeState> m_fallbackState;
  bool ValidateFallbackState(NodeState nodeState, NodeState statePropose);
};

#endif  // __NODE_H__
