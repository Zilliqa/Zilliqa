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

#ifndef __DIRECTORYSERVICE_H__
#define __DIRECTORYSERVICE_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <shared_mutex>
#include <vector>

#include "ShardStruct.h"
#include "common/Broadcastable.h"
#include "common/Executable.h"
#include "libConsensus/Consensus.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libLookup/Synchronizer.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerStore.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/TimeUtils.h"

class Mediator;

/// Implements Directory Service functionality including PoW verification, DS,
/// Tx Block Consensus and sharding management.
class DirectoryService : public Executable, public Broadcastable {
  std::chrono::system_clock::time_point m_timespec;

  enum Action {
    PROCESS_POWSUBMISSION = 0x00,
    VERIFYPOW,
    PROCESS_DSBLOCKCONSENSUS,
    PROCESS_MICROBLOCKSUBMISSION,
    PROCESS_FINALBLOCKCONSENSUS,
    PROCESS_VIEWCHANGECONSENSUS
  };

  std::mutex m_mutexConsensus;

  // Temporary buffers for sharding committee members and transaction sharing
  // assignments during DSBlock consensus
  std::vector<Peer> m_tempDSReceivers;
  std::vector<std::vector<Peer>> m_tempShardReceivers;
  std::vector<std::vector<Peer>> m_tempShardSenders;
  DequeOfShard m_tempShards;  // vector<vector<pair<PubKey, Peer>>>;
  std::map<PubKey, uint32_t> m_tempPublicKeyToshardIdMap;
  std::map<PubKey, uint16_t> m_tempMapNodeReputation;

  // PoW common variables
  std::mutex m_mutexAllPoWConns;
  std::map<PubKey, Peer> m_allPoWConns;

  std::mutex m_mutexAllPoWCounter;
  std::map<PubKey, uint8_t> m_AllPoWCounter;
  std::mutex m_mutexAllPOW;
  std::map<PubKey, std::array<unsigned char, 32>>
      m_allPoWs;  // map<pubkey, PoW Soln>
  std::mutex m_mutexAllDSPOWs;
  std::map<PubKey, std::array<unsigned char, 32>>
      m_allDSPoWs;  // map<pubkey, DS PoW Sol

  // Consensus variables
  std::shared_ptr<ConsensusCommon> m_consensusObject;
  std::vector<unsigned char> m_consensusBlockHash;

  // PoW (DS block) consensus variables
  std::shared_ptr<DSBlock> m_pendingDSBlock;
  std::mutex m_mutexPendingDSBlock;

  // Final block consensus variables
  std::shared_ptr<TxBlock> m_finalBlock;

  struct MBSubmissionBufferEntry {
    std::vector<MicroBlock> m_microBlocks;
    std::vector<unsigned char> m_stateDelta;
    MBSubmissionBufferEntry(const std::vector<MicroBlock>& microBlocks,
                            const std::vector<unsigned char>& stateDelta)
        : m_microBlocks(microBlocks), m_stateDelta(stateDelta) {}
  };
  std::mutex m_mutexMBSubmissionBuffer;
  std::unordered_map<uint64_t, std::vector<MBSubmissionBufferEntry>>
      m_MBSubmissionBuffer;

  std::mutex m_mutexFinalBlockConsensusBuffer;
  std::unordered_map<uint32_t,
                     std::vector<std::pair<Peer, std::vector<unsigned char>>>>
      m_finalBlockConsensusBuffer;

  std::mutex m_mutexCVMissingMicroBlock;
  std::condition_variable cv_MissingMicroBlock;

  // View Change
  std::atomic<uint32_t> m_viewChangeCounter;
  Peer m_candidateLeader;
  std::shared_ptr<VCBlock> m_pendingVCBlock;
  std::mutex m_mutexPendingVCBlock;
  std::condition_variable cv_ViewChangeConsensusObj;
  std::mutex m_MutexCVViewChangeConsensusObj;

  std::condition_variable cv_viewChangeDSBlock;
  std::mutex m_MutexCVViewChangeDSBlock;
  std::condition_variable cv_viewChangeFinalBlock;
  std::mutex m_MutexCVViewChangeFinalBlock;
  std::condition_variable cv_ViewChangeVCBlock;
  std::mutex m_MutexCVViewChangeVCBlock;

  // Consensus and consensus object
  std::condition_variable cv_DSBlockConsensus;
  std::mutex m_MutexCVDSBlockConsensus;
  std::condition_variable cv_DSBlockConsensusObject;
  std::mutex m_MutexCVDSBlockConsensusObject;
  std::condition_variable cv_POWSubmission;
  std::mutex m_MutexCVPOWSubmission;
  std::condition_variable cv_processConsensusMessage;
  std::mutex m_mutexProcessConsensusMessage;

  std::mutex m_mutexRunConsensusOnFinalBlock;

  // TO Remove
  Mediator& m_mediator;

  uint32_t m_numOfAbsentMicroBlocks;

  // Coinbase
  std::map<uint64_t, std::unordered_map<int32_t, std::vector<Address>>>
      m_coinbaseRewardees;
  std::mutex m_mutexCoinbaseRewardees;

  const uint32_t RESHUFFLE_INTERVAL = 500;

  // Message handlers
  bool ProcessSetPrimary(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);
  bool ProcessPoWSubmission(const std::vector<unsigned char>& message,
                            unsigned int offset, const Peer& from);
  bool ProcessDSBlockConsensus(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);
  bool ProcessMicroblockSubmission(const std::vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from);
  bool ProcessFinalBlockConsensus(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
  bool ProcessFinalBlockConsensusCore(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from);
  bool ProcessViewChangeConsensus(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  bool CheckState(Action action);

  // For PoW submission counter
  bool CheckPoWSubmissionExceedsLimitsForNode(const PubKey& key);
  void UpdatePoWSubmissionCounterforNode(const PubKey& key);
  void ResetPoWSubmissionCounter();
  void ClearReputationOfNodeWithoutPoW();
  std::set<PubKey> FindTopPriorityNodes();

  void SetupMulticastConfigForShardingStructure(unsigned int& my_DS_cluster_num,
                                                unsigned int& my_shards_lo,
                                                unsigned int& my_shards_hi);
  void SendEntireShardingStructureToShardNodes(unsigned int my_shards_lo,
                                               unsigned int my_shards_hi);

  unsigned int ComposeDSBlock(
      const std::vector<std::pair<std::array<unsigned char, 32>, PubKey>>&
          sortedDSPoWSolns,
      std::vector<std::pair<std::array<unsigned char, 32>, PubKey>>&
          sortedPoWSolns);
  void ComputeSharding(
      const std::vector<std::pair<std::array<unsigned char, 32>, PubKey>>&
          sortedPoWSolns);

  void ComputeTxnSharingAssignments(const std::vector<Peer>& proposedDSMembers);
  bool VerifyPoWOrdering(const DequeOfShard& shards);
  bool VerifyNodePriority(const DequeOfShard& shards);

  // internal calls from RunConsensusOnDSBlock
  bool RunConsensusOnDSBlockWhenDSPrimary();
  bool RunConsensusOnDSBlockWhenDSBackup();

  // internal calls from ProcessDSBlockConsensus
  void StoreDSBlockToStorage();  // To further refactor
  void SendDSBlockToLookupNodes();
  void SendDSBlockToNewDSLeader();
  void SetupMulticastConfigForDSBlock(unsigned int& my_DS_cluster_num,
                                      unsigned int& my_shards_lo,
                                      unsigned int& my_shards_hi) const;
  void SendDSBlockToShardNodes(const unsigned int my_shards_lo,
                               const unsigned int my_shards_hi);
  void UpdateMyDSModeAndConsensusId();
  void UpdateDSCommiteeComposition();

  void ProcessDSBlockConsensusWhenDone(
      const std::vector<unsigned char>& message, unsigned int offset);

  // internal calls from ProcessFinalBlockConsensus
  bool SendFinalBlockToLookupNodes();
  void ProcessFinalBlockConsensusWhenDone();

  void SendFinalBlockToShardNodes(unsigned int my_DS_cluster_num,
                                  unsigned int my_shards_lo,
                                  unsigned int my_shards_hi);
  void CommitFinalBlockConsensusBuffer();

  // Final Block functions
  bool RunConsensusOnFinalBlockWhenDSPrimary();
  bool RunConsensusOnFinalBlockWhenDSBackup();
  void ComposeFinalBlock();
  bool CheckWhetherDSBlockIsFresh(const uint64_t dsblock_num);
  void CommitMBSubmissionMsgBuffer();
  bool ProcessMicroblockSubmissionFromShard(
      const uint64_t blockNumber, const std::vector<MicroBlock>& microBlocks,
      const std::vector<unsigned char>& stateDelta);
  bool ProcessMicroblockSubmissionFromShardCore(
      const std::vector<MicroBlock>& microBlocks,
      const std::vector<unsigned char>& stateDelta);
  bool ProcessMissingMicroblockSubmission(
      const uint64_t blockNumber, const std::vector<MicroBlock>& microBlocks,
      const std::vector<unsigned char>& stateDelta);
  void ExtractDataFromMicroblocks(
      TxnHash& microblockTxnTrieRoot, StateHash& microblockDeltaTrieRoot,
      TxnHash& microblockTranReceiptRoot,
      std::vector<MicroBlockHashSet>& microblockHashes,
      std::vector<uint32_t>& shardIds,
      boost::multiprecision::uint256_t& allGasLimit,
      boost::multiprecision::uint256_t& allGasUsed, uint32_t& numTxs,
      std::vector<bool>& isMicroBlockEmpty, uint32_t& numMicroBlocks);
  bool VerifyMicroBlockCoSignature(const MicroBlock& microBlock,
                                   uint32_t shardId);
  bool ProcessStateDelta(const std::vector<unsigned char>& stateDelta,
                         const StateHash& microBlockStateDeltaHash);

  // FinalBlockValidator functions
  bool CheckFinalBlockValidity(std::vector<unsigned char>& errorMsg);
  bool CheckBlockTypeIsFinal();
  bool CheckFinalBlockVersion();
  bool CheckPreviousFinalBlockHash();
  bool CheckFinalBlockNumber();
  bool CheckFinalBlockTimestamp();
  bool CheckMicroBlocks(std::vector<unsigned char>& errorMsg);
  bool CheckMicroBlockHashRoot();
  bool CheckIsMicroBlockEmpty();
  bool CheckStateRoot();
  bool CheckStateDeltaHash();
  void LoadUnavailableMicroBlocks();

  // Redundant code
  // bool WaitForTxnBodies();

  // DS block consensus validator function
  bool DSBlockValidator(const std::vector<unsigned char>& message,
                        unsigned int offset,
                        std::vector<unsigned char>& errorMsg,
                        const uint32_t consensusID, const uint64_t blockNumber,
                        const std::vector<unsigned char>& blockHash,
                        const uint16_t leaderID, const PubKey& leaderKey,
                        std::vector<unsigned char>& messageToCosign);

  // Sharding consensus validator function
  bool ShardingValidator(const std::vector<unsigned char>& sharding_structure,
                         std::vector<unsigned char>& errorMsg);

  // Final block consensus validator function
  bool FinalBlockValidator(const std::vector<unsigned char>& message,
                           unsigned int offset,
                           std::vector<unsigned char>& errorMsg,
                           const uint32_t consensusID,
                           const uint64_t blockNumber,
                           const std::vector<unsigned char>& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           std::vector<unsigned char>& messageToCosign);

  // View change consensus validator function
  bool ViewChangeValidator(const std::vector<unsigned char>& message,
                           unsigned int offset,
                           std::vector<unsigned char>& errorMsg,
                           const uint32_t consensusID,
                           const uint64_t blockNumber,
                           const std::vector<unsigned char>& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           std::vector<unsigned char>& messageToCosign);

  void StoreFinalBlockToDisk();

  bool OnNodeMissingMicroBlocks(const std::vector<unsigned char>& errorMsg,
                                const Peer& from);

  // void StoreMicroBlocksToDisk();

  // Used to reconsile view of m_AllPowConn is different.
  void LastDSBlockRequest();

  bool ProcessLastDSBlockRequest(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
  bool ProcessLastDSBlockResponse(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);

  // View change
  void SetLastKnownGoodState();
  void RunConsensusOnViewChange();
  void ScheduleViewChangeTimeout();
  void ComputeNewCandidateLeader();
  bool RunConsensusOnViewChangeWhenCandidateLeader();
  bool RunConsensusOnViewChangeWhenNotCandidateLeader();
  void ProcessViewChangeConsensusWhenDone();
  void ProcessNextConsensus(unsigned char viewChangeState);

  // Reset certain variables to the initial state
  bool CleanVariables();

  void CleanFinalblockConsensusBuffer();

  uint8_t CalculateNewDifficulty(const uint8_t& currentDifficulty);
  uint8_t CalculateNewDSDifficulty(const uint8_t& dsDifficulty);
  uint64_t CalculateNumberOfBlocksPerYear() const;

 public:
  enum Mode : unsigned char { IDLE = 0x00, PRIMARY_DS, BACKUP_DS };

  enum DirState : unsigned char {
    POW_SUBMISSION = 0x00,
    DSBLOCK_CONSENSUS_PREP,
    DSBLOCK_CONSENSUS,
    MICROBLOCK_SUBMISSION,
    FINALBLOCK_CONSENSUS_PREP,
    FINALBLOCK_CONSENSUS,
    VIEWCHANGE_CONSENSUS_PREP,
    VIEWCHANGE_CONSENSUS,
    ERROR
  };

  /// Transaction sharing assignments
  std::vector<Peer> m_DSReceivers;
  std::vector<std::vector<Peer>> m_shardReceivers;
  std::vector<std::vector<Peer>> m_shardSenders;

  enum SUBMITMICROBLOCKTYPE : unsigned char {
    SHARDMICROBLOCK = 0x00,
    MISSINGMICROBLOCK = 0x01
  };

  /// Sharing assignment for state delta
  std::vector<Peer> m_sharingAssignment;

  uint16_t m_consensusLeaderID;

  std::mutex m_MutexScheduleDSMicroBlockConsensus;
  std::condition_variable cv_scheduleDSMicroBlockConsensus;

  std::mutex m_MutexScheduleFinalBlockConsensus;
  std::condition_variable cv_scheduleFinalBlockConsensus;

  /// The current role of this Zilliqa instance within the directory service
  /// committee.
  std::atomic<Mode> m_mode;

  // Sharding committee members
  std::mutex m_mutexShards;
  DequeOfShard m_shards;
  std::map<PubKey, uint32_t> m_publicKeyToshardIdMap;

  // Proof of Reputation(PoR) variables.
  std::map<PubKey, uint16_t> m_mapNodeReputation;

  /// The current internal state of this DirectoryService instance.
  std::atomic<DirState> m_state;

  /// The state (before view change) of this DirectoryService instance.
  std::atomic<DirState> m_viewChangestate;

  /// The ID number of this Zilliqa instance for use with consensus operations.
  uint16_t m_consensusMyID;

  /// The epoch number when DS tries doing Rejoin
  uint64_t m_latestActiveDSBlockNum = 0;

  /// Serialized account store temp to revert to if ds microblock consensus
  /// failed
  std::vector<unsigned char> m_stateDeltaFromShards;
  std::vector<unsigned char> m_stateDeltaWhenRunDSMB;

  /// Whether to send txn from ds microblock to lookup at finalblock consensus
  /// done
  std::atomic<bool> m_toSendTxnToLookup;

  /// Whether ds started microblock consensus
  std::atomic<bool> m_dsStartedMicroblockConsensus;

  /// Whether ds started finalblock consensus
  std::mutex m_mutexPrepareRunFinalblockConsensus;
  std::atomic<bool> m_startedRunFinalblockConsensus;

  std::unordered_map<uint64_t, std::set<MicroBlock>> m_microBlocks;
  std::mutex m_mutexMicroBlocks;

  Synchronizer m_synchronizer;

  /// Constructor. Requires mediator reference to access Node and other global
  /// members.
  DirectoryService(Mediator& mediator);

  /// Destructor.
  ~DirectoryService();

  /// Sets the value of m_state.
  void SetState(DirState state);

  /// Start synchronization with lookup as a DS node
  void StartSynchronization();

  /// Implements the GetBroadcastList function inherited from Broadcastable.
  std::vector<Peer> GetBroadcastList(unsigned char ins_type,
                                     const Peer& broadcast_originator);

  /// Launches separate thread to execute sharding consensus after wait_window
  /// seconds.
  void ScheduleShardingConsensus(const unsigned int wait_window);

  /// Rejoin the network as a DS node in case of failure happens in protocol
  void RejoinAsDS();

  /// Post processing after the DS node successfully synchronized with the
  /// network
  bool FinishRejoinAsDS();

  void RunConsensusOnFinalBlock(bool revertStateDelta = false);

  // Coinbase
  bool SaveCoinbase(const std::vector<bool>& b1, const std::vector<bool>& b2,
                    const int32_t& shard_id);
  void InitCoinbase();

  template <class Container>
  bool SaveCoinbaseCore(const std::vector<bool>& b1,
                        const std::vector<bool>& b2, const Container& shard,
                        const uint32_t& shard_id);

  /// Implements the Execute function inherited from Executable.
  bool Execute(const std::vector<unsigned char>& message, unsigned int offset,
               const Peer& from);

  /// Used by PoW winner to configure sharding variables as the next DS leader
  bool ProcessShardingStructure(
      const DequeOfShard& shards,
      std::map<PubKey, uint32_t>& publicKeyToshardIdMap,
      std::map<PubKey, uint16_t>& mapNodeReputation);

  /// Used by PoW winner to configure txn sharing assignment variables as the
  /// next DS leader
  void ProcessTxnBodySharingAssignment();

  /// Used by PoW winner to finish setup as the next DS leader
  void StartFirstTxEpoch();

  void DetermineShardsToSendBlockTo(unsigned int& my_DS_cluster_num,
                                    unsigned int& my_shards_lo,
                                    unsigned int& my_shards_hi);
  void SendBlockToShardNodes(unsigned int my_DS_cluster_num,
                             unsigned int my_shards_lo,
                             unsigned int my_shards_hi,
                             std::vector<unsigned char>& block_message);

  /// Begin next round of DS consensus
  void StartNewDSEpochConsensus(bool fromFallback = false);

  static uint8_t CalculateNewDifficultyCore(
      uint8_t currentDifficulty, uint8_t minDifficulty, int64_t currentNodes,
      int64_t powSubmissions, int64_t expectedNodes,
      uint32_t maxAdjustThreshold, int64_t currentEpochNum,
      int64_t numBlockPerYear);

  /// Calculate node priority to determine which node has the priority to join
  /// the network.
  static uint8_t CalculateNodePriority(uint16_t reputation);

  /// PoW (DS block) consensus functions
  void RunConsensusOnDSBlock(bool isRejoin = false);

 private:
  static std::map<DirState, std::string> DirStateStrings;
  std::string GetStateString() const;
  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
  bool ValidateViewChangeState(DirState NodeState, DirState StatePropose);

  void AddDSPoWs(PubKey Pubk, std::array<unsigned char, 32> DSPOWSoln);
  std::map<PubKey, std::array<unsigned char, 32>> GetAllDSPoWs();
  void ClearDSPoWSolns();
  std::array<unsigned char, 32> GetDSPoWSoln(PubKey Pubk);
  bool IsNodeSubmittedDSPoWSoln(PubKey Pubk);
  uint32_t GetNumberOfDSPoWSolns();
};

#endif  // __DIRECTORYSERVICE_H__
