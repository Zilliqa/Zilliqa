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

#ifndef ZILLIQA_SRC_LIBDIRECTORYSERVICE_DIRECTORYSERVICE_H_
#define ZILLIQA_SRC_LIBDIRECTORYSERVICE_DIRECTORYSERVICE_H_

#include <array>
#include <condition_variable>
#include <map>
#include <set>
#include <shared_mutex>

#include <Schnorr.h>
#include "common/Constants.h"
#include "libBlockchain/Block.h"
#include "libBlockchain/BlockHashSet.h"
#include "libConsensus/Consensus.h"
#include "libData/MiningData/DSPowSolution.h"
#include "libLookup/Synchronizer.h"
#include "libNetwork/DataSender.h"
#include "libNetwork/Executable.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/ShardStruct.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/TimeUtils.h"

class Mediator;

struct PoWSolution {
  uint64_t m_nonce;
  std::array<unsigned char, 32> m_result;
  std::array<unsigned char, 32> m_mixhash;
  uint32_t m_lookupId;
  uint128_t m_gasPrice;
  GovProposalIdVotePair m_govProposal;  // proposal id and vote value pair

  PoWSolution()
      : m_nonce(0),
        m_result({{0}}),
        m_mixhash({{0}}),
        m_lookupId(uint32_t() - 1),
        m_gasPrice(0),
        m_govProposal({0, 0}) {

  }  // The oldest DS (and now new shard node) will have this default value
  PoWSolution(const uint64_t n, const std::array<unsigned char, 32>& r,
              const std::array<unsigned char, 32>& m, uint32_t l,
              const uint128_t& gp, const std::pair<uint32_t, uint32_t>& gvp)
      : m_nonce(n),
        m_result(r),
        m_mixhash(m),
        m_lookupId(l),
        m_gasPrice(gp),
        m_govProposal(gvp) {}
  bool operator==(const PoWSolution& rhs) const {
    return std::tie(m_nonce, m_result, m_mixhash, m_lookupId, m_gasPrice,
                    m_govProposal) ==
           std::tie(rhs.m_nonce, rhs.m_result, rhs.m_mixhash, rhs.m_lookupId,
                    rhs.m_gasPrice, rhs.m_govProposal);
  }
};

struct DSGuardUpdateStruct {
  PubKey m_dsGuardPubkey;
  Peer m_dsGuardNewNetworkInfo;
  uint64_t m_timestamp;

  DSGuardUpdateStruct()
      : m_dsGuardPubkey(PubKey()),
        m_dsGuardNewNetworkInfo(Peer()),
        m_timestamp(0) {}

  DSGuardUpdateStruct(const PubKey& curDSGuardPubkey,
                      const Peer& newDSGuardNetworkInfo,
                      const uint64_t timestampOfChangeRequest)
      : m_dsGuardPubkey(curDSGuardPubkey),
        m_dsGuardNewNetworkInfo(newDSGuardNetworkInfo),
        m_timestamp(timestampOfChangeRequest) {}

  bool operator==(const DSGuardUpdateStruct& rhs) const {
    return std::tie(m_dsGuardPubkey, m_dsGuardNewNetworkInfo, m_timestamp) ==
           std::tie(rhs.m_dsGuardPubkey, rhs.m_dsGuardNewNetworkInfo,
                    rhs.m_timestamp);
  }
};

namespace CoinbaseReward {
const int FINALBLOCK_REWARD = -1;
const int LOOKUP_REWARD = -2;
}  // namespace CoinbaseReward

using VectorOfPoWSoln =
    std::vector<std::pair<std::array<unsigned char, 32>, PubKey>>;
using MapOfPubKeyPoW = std::map<PubKey, PoWSolution>;

/// Implements Directory Service functionality including PoW verification, DS,
/// Tx Block Consensus and sharding management.
class DirectoryService : public Executable {
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
  DequeOfShard m_tempShards;
  std::map<PubKey, uint32_t> m_tempPublicKeyToshardIdMap;
  std::map<PubKey, uint16_t> m_tempMapNodeReputation;

  // PoW common variables
  std::mutex m_mutexAllPoWConns;
  std::map<PubKey, Peer> m_allPoWConns;

  std::mutex m_mutexAllPoWCounter;
  std::map<PubKey, uint8_t> m_AllPoWCounter;

  mutable std::mutex m_mutexAllPOW;
  MapOfPubKeyPoW m_allPoWs;  // map<pubkey, PoW Soln>

  std::mutex m_mutexAllDSPOWs;
  MapOfPubKeyPoW m_allDSPoWs;  // map<pubkey, DS PoW Sol

  // Consensus variables
  std::shared_ptr<ConsensusCommon> m_consensusObject;
  zbytes m_consensusBlockHash;
  std::mutex m_mutexCommitFailure;

  // PoW (DS block) consensus variables
  std::shared_ptr<DSBlock> m_pendingDSBlock;
  std::mutex m_mutexPendingDSBlock;

  // Final block consensus variables
  std::shared_ptr<TxBlock> m_finalBlock;
  bool m_completeFinalBlockReady = false;
  std::condition_variable m_cvCompleteFinalBlockReady;

  struct MBSubmissionBufferEntry {
    MicroBlock m_microBlock;
    zbytes m_stateDelta;
    MBSubmissionBufferEntry(const MicroBlock& microBlock,
                            const zbytes& stateDelta)
        : m_microBlock(microBlock), m_stateDelta(stateDelta) {}
  };
  std::mutex m_mutexMBSubmissionBuffer;
  std::unordered_map<uint64_t, std::vector<MBSubmissionBufferEntry>>
      m_MBSubmissionBuffer;

  std::mutex m_mutexFinalBlockConsensusBuffer;
  std::unordered_map<uint32_t, VectorOfNodeMsg> m_finalBlockConsensusBuffer;

  std::mutex m_mutexCVMissingMicroBlock;
  std::condition_variable cv_MissingMicroBlock;

  // View Change
  std::atomic<uint16_t> m_candidateLeaderIndex{};
  VectorOfNode m_cumulativeFaultyLeaders;
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

  // To be used to store vc block (ds block consensus) for "normal nodes"
  std::mutex m_mutexVCBlockVector;
  std::vector<VCBlock> m_VCBlockVector;

  // Consensus and consensus object
  std::condition_variable cv_DSBlockConsensus;
  std::mutex m_MutexCVDSBlockConsensus;
  std::condition_variable cv_DSBlockConsensusObject;
  std::mutex m_MutexCVDSBlockConsensusObject;
  std::condition_variable cv_POWSubmission;
  std::mutex m_MutexCVPOWSubmission;
  std::condition_variable cv_processConsensusMessage;
  std::mutex m_mutexProcessConsensusMessage;

  std::atomic<uint16_t> m_consensusLeaderID{};
  /// The ID number of this Zilliqa instance for use with consensus operations.
  std::atomic<uint16_t> m_consensusMyID{};

  std::mutex m_mutexRunConsensusOnFinalBlock;

  Mediator& m_mediator;

  // Coinbase
  // Map<EpochNumber, Map<shard-id, vector <Public keys to be rewarded>>
  std::map<uint64_t, std::map<int32_t, std::vector<PubKey>>>
      m_coinbaseRewardees;
  std::mutex m_mutexCoinbaseRewardees;

  // DS Reputation
  // Map<Public Key, Number of Co-Sigs> observed from the coinbase rewards.
  std::map<PubKey, uint32_t> m_dsMemberPerformance;
  std::mutex m_mutexDsMemberPerformance;

  // pow solutions
  std::vector<DSPowSolution> m_powSolutions;
  std::mutex m_mutexPowSolution;

  const uint32_t RESHUFFLE_INTERVAL = 500;

  // Message handlers
  bool ProcessSetPrimary(const zbytes& message, unsigned int offset,
                         const Peer& from,
                         [[gnu::unused]] const unsigned char& startByte);
  bool ProcessPoWSubmission(const zbytes& message, unsigned int offset,
                            const Peer& from,
                            [[gnu::unused]] const unsigned char& startByte);
  bool ProcessPoWPacketSubmission(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool VerifyPoWSubmission(const DSPowSolution& sol);

  bool ProcessDSBlockConsensus(const zbytes& message, unsigned int offset,
                               const Peer& from,
                               [[gnu::unused]] const unsigned char& startByte);
  bool ProcessMicroblockSubmission(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessFinalBlockConsensus(const zbytes& message, unsigned int offset,
                                  const Peer& from,
                                  const unsigned char& startByte);
  bool ProcessFinalBlockConsensusCore(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessViewChangeConsensus(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessPushLatestDSBlock(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);
  bool ProcessPushLatestTxBlock(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);
  bool ProcessVCPushLatestDSTxBlock(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessNewDSGuardNetworkInfo(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  // Get cosig and rewards for given epoch
  bool ProcessCosigsRewardsFromSeed(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetDSLeaderTxnPool(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  bool CheckState(Action action);

  bool CheckSolnFromNonDSCommittee(const PubKey& submitterPubKey,
                                   const Peer& submitterPeer);

  // For PoW submission counter
  bool CheckPoWSubmissionExceedsLimitsForNode(const PubKey& key);
  void UpdatePoWSubmissionCounterforNode(const PubKey& key);
  void ResetPoWSubmissionCounter();
  void ClearReputationOfNodeWithoutPoW();
  static void RemoveReputationOfNodeFailToJoin(
      const DequeOfShard& shards,
      std::map<PubKey, uint16_t>& mapNodeReputation);
  std::set<PubKey> FindTopPriorityNodes(uint8_t& lowestPriority);

  void SetupMulticastConfigForShardingStructure(unsigned int& my_DS_cluster_num,
                                                unsigned int& my_shards_lo,
                                                unsigned int& my_shards_hi);
  void SendEntireShardingStructureToShardNodes(unsigned int my_shards_lo,
                                               unsigned int my_shards_hi);

  unsigned int ComputeDSBlockParameters(const VectorOfPoWSoln& sortedDSPoWSolns,
                                        std::map<PubKey, Peer>& powDSWinners,
                                        MapOfPubKeyPoW& dsWinnerPoWs,
                                        uint8_t& dsDifficulty,
                                        uint8_t& difficulty, uint64_t& blockNum,
                                        BlockHash& prevHash);
  void ComputeSharding(const VectorOfPoWSoln& sortedPoWSolns);
  void InjectPoWForDSNode(VectorOfPoWSoln& sortedPoWSolns,
                          unsigned int numOfProposedDSMembers,
                          const std::vector<PubKey>& removeDSNodePubkeys);

  // Gas Pricer
  uint128_t GetNewGasPrice();
  uint128_t GetHistoricalMeanGasPrice();
  uint128_t GetDecreasedGasPrice();
  uint128_t GetIncreasedGasPrice();
  bool VerifyGasPrice(const uint128_t& gasPrice);

  bool VerifyPoWWinner(const MapOfPubKeyPoW& dsWinnerPoWsFromLeader);
  bool VerifyDifficulty();
  bool VerifyRemovedByzantineNodes();
  bool VerifyPoWOrdering(const DequeOfShard& shards,
                         const MapOfPubKeyPoW& allPoWsFromLeader,
                         const MapOfPubKeyPoW& priorityNodePoWs);
  bool VerifyPoWFromLeader(const Peer& peer, const PubKey& pubKey,
                           const PoWSolution& powSoln);
  bool VerifyNodePriority(const DequeOfShard& shards,
                          MapOfPubKeyPoW& priorityNodePoWs);

  // DS Reputation
  void SaveDSPerformance();
  unsigned int DetermineByzantineNodes(
      unsigned int numOfProposedDSMembers,
      std::vector<PubKey>& removeDSNodePubkeys);

  // internal calls from RunConsensusOnDSBlock
  bool WaitUntilCompleteFinalBlockIsReady();
  bool RunConsensusOnDSBlockWhenDSPrimary();
  bool RunConsensusOnDSBlockWhenDSBackup();

  // internal calls from ProcessDSBlockConsensus
  bool StoreDSBlockToStorage();  // To further refactor
  bool ComposeDSBlockMessageForSender(zbytes& dsblock_message);
  void SendDSBlockToLookupNodesAndNewDSMembers(const zbytes& dsblock_message);
  void SendDSBlockToShardNodes(const zbytes& dsblock_message,
                               const DequeOfShard& shards,
                               const unsigned int& my_shards_lo,
                               const unsigned int& my_shards_hi);
  void UpdateMyDSModeAndConsensusId();
  void UpdateDSCommitteeComposition();

  void ProcessDSBlockConsensusWhenDone();

  // internal calls from ProcessFinalBlockConsensus
  bool ComposeFinalBlockMessageForSender(zbytes& finalblock_message);
  void ProcessFinalBlockConsensusWhenDone();
  void CommitFinalBlockConsensusBuffer();

  // Final Block functions
  bool RunConsensusOnFinalBlockWhenDSBackup();
  bool ComposeFinalBlock();
  bool CheckWhetherDSBlockIsFresh(const uint64_t dsblock_num);
  void CommitMBSubmissionMsgBuffer();
  bool ProcessMicroblockSubmissionFromShard(
      const uint64_t epochNumber, const std::vector<MicroBlock>& microBlocks,
      const std::vector<zbytes>& stateDelta);
  bool ProcessMicroblockSubmissionFromShardCore(const MicroBlock& microBlocks,
                                                const zbytes& stateDelta);
  bool ProcessMissingMicroblockSubmission(
      const uint64_t epochNumber, const std::vector<MicroBlock>& microBlocks,
      const std::vector<zbytes>& stateDeltas);
  void ExtractDataFromMicroblocks(std::vector<MicroBlockInfo>& mbInfos,
                                  uint64_t& allGasLimit, uint64_t& allGasUsed,
                                  uint128_t& allRewards, uint32_t& numTxs);
  bool ProcessStateDelta(const zbytes& stateDelta,
                         const StateHash& microBlockStateDeltaHash,
                         const BlockHash& microBlockHash);
  void SkipDSMicroBlock();
  void PrepareRunConsensusOnFinalBlockNormal();

  // FinalBlockValidator functions
  bool CheckBlockHash();
  bool CheckFinalBlockValidity(zbytes& errorMsg);
  bool CheckMicroBlockValidity(zbytes& errorMsg);
  bool CheckFinalBlockVersion();
  bool CheckPreviousFinalBlockHash();
  bool CheckFinalBlockNumber();
  bool CheckFinalBlockTimestamp();
  bool CheckMicroBlocks(zbytes& errorMsg, bool fromShards,
                        bool generateErrorMsg);
  bool CheckLegitimacyOfMicroBlocks();
  bool CheckMicroBlockInfo();
  bool CheckStateRoot();
  bool CheckStateDeltaHash();
  void LoadUnavailableMicroBlocks();

  // DS block consensus validator function
  bool DSBlockValidator(const zbytes& message, unsigned int offset,
                        zbytes& errorMsg, const uint32_t consensusID,
                        const uint64_t blockNumber, const zbytes& blockHash,
                        const uint16_t leaderID, const PubKey& leaderKey,
                        zbytes& messageToCosign);

  // Sharding consensus validator function
  bool ShardingValidator(const zbytes& sharding_structure, zbytes& errorMsg);

  // PrePrep Final block consensus validator function
  bool PrePrepFinalBlockValidator(const zbytes& message, unsigned int offset,
                                  zbytes& errorMsg, const uint32_t consensusID,
                                  const uint64_t blockNumber,
                                  const zbytes& blockHash,
                                  const uint16_t leaderID,
                                  const PubKey& leaderKey,
                                  zbytes& messageToCosign);

  // Final block consensus validator function
  bool FinalBlockValidator(const zbytes& message, unsigned int offset,
                           zbytes& errorMsg, const uint32_t consensusID,
                           const uint64_t blockNumber, const zbytes& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           zbytes& messageToCosign);

  // View change consensus validator function
  bool ViewChangeValidator(const zbytes& message, unsigned int offset,
                           zbytes& errorMsg, const uint32_t consensusID,
                           const uint64_t blockNumber, const zbytes& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           zbytes& messageToCosign);
  bool StoreFinalBlockToDisk();

  bool OnNodeFinalConsensusError(const zbytes& errorMsg, const Peer& from);
  bool OnNodeMissingMicroBlocks(const zbytes& errorMsg,
                                const unsigned int offset, const Peer& from);

  // void StoreMicroBlocksToDisk();

  // Used to reconsile view of m_AllPowConn is different.
  void LastDSBlockRequest();

  bool ProcessLastDSBlockRequest(const zbytes& message, unsigned int offset,
                                 const Peer& from);
  bool ProcessLastDSBlockResponse(const zbytes& message, unsigned int offset,
                                  const Peer& from);

  // View change
  bool NodeVCPrecheck();
  void SetLastKnownGoodState();
  void RunConsensusOnViewChange();
  void ScheduleViewChangeTimeout();
  bool ComputeNewCandidateLeader(const uint16_t candidateLeaderIndex);
  uint16_t CalculateNewLeaderIndex();
  bool RunConsensusOnViewChangeWhenCandidateLeader(
      const uint16_t candidateLeaderIndex);
  bool RunConsensusOnViewChangeWhenNotCandidateLeader(
      const uint16_t candidateLeaderIndex);
  void ProcessViewChangeConsensusWhenDone();
  void ProcessNextConsensus(unsigned char viewChangeState);

  bool VCFetchLatestDSTxBlockFromSeedNodes();
  zbytes ComposeVCGetDSTxBlockMessage();
  bool ComposeVCBlockForSender(zbytes& vcblock_message);
  void CleanUpViewChange(bool isPrecheckFail);

  void AddToFinalBlockConsensusBuffer(uint32_t consensusId,
                                      const zbytes& message,
                                      unsigned int offset, const Peer& peer,
                                      const PubKey& senderPubKey);
  void CleanFinalBlockConsensusBuffer();

  uint8_t CalculateNewDifficulty(const uint8_t& currentDifficulty);
  uint8_t CalculateNewDSDifficulty(const uint8_t& dsDifficulty);
  void CalculateCurrentDSMBGasLimit();

  void ReloadGuardedShards(DequeOfShard& shards);

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
    ERROR,
    SYNC
  };

  enum SUBMITMICROBLOCKTYPE : unsigned char {
    SHARDMICROBLOCK = 0x00,
    MISSINGMICROBLOCK = 0x01
  };

  enum FINALCONSENSUSERRORTYPE : unsigned char {
    CHECKMICROBLOCK = 0x00,
    DSMBMISSINGTXN = 0x01,
    CHECKFINALBLOCK = 0x02,
    DSFBMISSINGMB = 0x03
  };

  /// whether should accept pow submissions from miner
  std::atomic_bool m_powSubmissionWindowExpired = {false};

  /// Sharing assignment for state delta
  VectorOfPeer m_sharingAssignment;

  std::mutex m_MutexScheduleDSMicroBlockConsensus;
  std::condition_variable cv_scheduleDSMicroBlockConsensus;

  std::mutex m_MutexScheduleFinalBlockConsensus;
  std::condition_variable cv_scheduleFinalBlockConsensus;

  /// The current role of this Zilliqa instance within the directory service
  /// committee.
  std::atomic<Mode> m_mode{};

  // Sharding committee members
  std::mutex mutable m_mutexShards;
  DequeOfShard m_shards;
  std::map<PubKey, uint32_t> m_publicKeyToshardIdMap;

  // Proof of Reputation(PoR) variables.
  std::mutex mutable m_mutexMapNodeReputation;
  std::map<PubKey, uint16_t> m_mapNodeReputation;

  /// The current internal state of this DirectoryService instance.
  std::atomic<DirState> m_state{};

  /// The state (before view change) of this DirectoryService instance.
  std::atomic<DirState> m_viewChangestate{};

  /// The counter of viewchange happened during current epoch
  std::atomic<uint32_t> m_viewChangeCounter{};

  /// The epoch number when DS tries doing Rejoin
  uint64_t m_latestActiveDSBlockNum = 0;

  /// Serialized account store temp to revert to if ds microblock consensus
  /// failed
  zbytes m_stateDeltaFromShards;

  /// Whether ds started microblock consensus
  std::atomic<bool> m_stopRecvNewMBSubmission{};

  /// Whether ds started finalblock consensus
  std::mutex m_mutexPrepareRunFinalblockConsensus;

  struct MicroBlockCompare final {
    bool operator()(const MicroBlock& lhs, const MicroBlock& rhs) const;
  };

  std::mutex m_mutexMicroBlocks;
  // TODO: is it possible to use an unordered data structure instead of a set of
  // MicroBlocks?
  std::unordered_map<uint64_t, std::set<MicroBlock, MicroBlockCompare>>
      m_microBlocks;
  std::unordered_map<uint64_t, std::vector<BlockHash>> m_missingMicroBlocks;
  std::unordered_map<uint64_t, std::unordered_map<BlockHash, zbytes>>
      m_microBlockStateDeltas;
  uint128_t m_totalTxnFees;

  Synchronizer m_synchronizer;

  // For view change pre check
  std::vector<DSBlock> m_vcPreCheckDSBlocks;
  std::vector<TxBlock> m_vcPreCheckTxBlocks;
  std::mutex m_MutexCVViewChangePrecheckBlocks;

  std::mutex m_MutexCVViewChangePrecheck;
  std::condition_variable cv_viewChangePrecheck;

  // Guard mode recovery. currently used only by lookup node.
  std::mutex m_mutexLookupStoreForGuardNodeUpdate;
  std::map<uint64_t, std::vector<DSGuardUpdateStruct>>
      m_lookupStoreForGuardNodeUpdate;
  std::atomic_bool m_awaitingToSubmitNetworkInfoUpdate = {false};

  // For saving cosig and rewards
  std::mutex m_mutexLookupStoreCosigRewards;

  bool m_doRejoinAtDSConsensus = false;
  bool m_doRejoinAtFinalConsensus = false;

  // Indicate if its dsguard and its pod got restarted.
  bool m_dsguardPodDelete = false;

  // Indicate if the current dsepoch is one after node is upgraded
  bool m_dsEpochAfterUpgrade = false;

  // GetShards
  uint32_t GetNumShards() const;
  /// Force multicast when sending block to shard
  std::atomic<bool> m_forceMulticast{};

  /// microblock_gas_limit to be adjusted due to vc
  uint64_t m_microBlockGasLimit = DS_MICROBLOCK_GAS_LIMIT;

  /// Constructor. Requires mediator reference to access Node and other global
  /// members.
  DirectoryService(Mediator& mediator);

  /// Destructor.
  ~DirectoryService();

  /// Sets the value of m_state.
  void SetState(DirState state);

  // Set m_consensusMyID
  void SetConsensusMyID(uint16_t);

  // Get m_consensusMyID
  uint16_t GetConsensusMyID() const;

  // Set m_consensusLeaderID
  void SetConsensusLeaderID(uint16_t);

  // Get m_consensusLeaderID
  uint16_t GetConsensusLeaderID() const;

  // Increment m_consensusMyID
  void IncrementConsensusMyID();

  /// Start synchronization with lookup as a DS node
  void StartSynchronization(bool clean = true);

  /// Launches separate thread to execute sharding consensus after wait_window
  /// seconds.
  void ScheduleShardingConsensus(const unsigned int wait_window);

  /// Rejoin the network as a DS node in case of failure happens in protocol
  void RejoinAsDS(bool modeCheck = true, bool fromUpperSeed = false);

  /// Post processing after the DS node successfully synchronized with the
  /// network
  bool FinishRejoinAsDS(bool fetchShardingStruct = false);

  void RunConsensusOnFinalBlock();

  // Coinbase
  bool SaveCoinbase(const std::vector<bool>& b1, const std::vector<bool>& b2,
                    const int32_t& shard_id, const uint64_t& epochNum);
  void InitCoinbase();
  void StoreCoinbaseInDiagnosticDB(const DiagnosticDataCoinbase& entry);

  template <class Container>
  bool SaveCoinbaseCore(const std::vector<bool>& b1,
                        const std::vector<bool>& b2, const Container& shard,
                        const int32_t& shard_id, const uint64_t& epochNum);

  void GetCoinbaseRewardees(
      std::map<uint64_t, std::map<int32_t, std::vector<PubKey>>>&
          coinbase_rewardees);

  /// Implements the Execute function inherited from Executable.
  bool Execute(const zbytes& message, unsigned int offset, const Peer& from,
               const unsigned char& startByte);

  /// Used by PoW winner to configure sharding variables as the next DS leader
  bool ProcessShardingStructure(
      const DequeOfShard& shards,
      std::map<PubKey, uint32_t>& publicKeyToshardIdMap,
      std::map<PubKey, uint16_t>& mapNodeReputation);

  /// Used by PoW winner to finish setup as the next DS leader
  void StartFirstTxEpoch();

  /// Used by rejoined DS node
  void StartNextTxEpoch();

  /// Begin next round of DS consensus
  void StartNewDSEpochConsensus(bool isRejoin = false);

  static uint8_t CalculateNewDifficultyCore(uint8_t currentDifficulty,
                                            uint8_t minDifficulty,
                                            int64_t powSubmissions,
                                            int64_t expectedNodes,
                                            uint32_t powChangeoAdj);

  /// Calculate node priority to determine which node has the priority to join
  /// the network.
  static uint8_t CalculateNodePriority(uint16_t reputation);

  /// PoW (DS block) consensus functions
  void RunConsensusOnDSBlock();
  bool IsDSBlockVCState(unsigned char vcBlockState);

  // Sort the PoW submissions
  VectorOfPoWSoln SortPoWSoln(const MapOfPubKeyPoW& pows,
                              bool trimBeyondCommSize = false,
                              unsigned int byzantineRemoved = 0);
  int64_t GetAllPoWSize() const;

  bool SendPoWPacketSubmissionToOtherDSComm();

  // Reset certain variables to the initial state
  bool CleanVariables();

  // For DS guard to update it's network information while in GUARD_MODE
  bool UpdateDSGuardIdentity();

  // Update shard node's network info
  bool UpdateShardNodeNetworkInfo(const Peer& shardNodeNetworkInfo,
                                  const PubKey& pubKey);

  bool CheckIfShardNode(const PubKey& submitterPubKey);

  // Get entire network peer info
  void GetEntireNetworkPeerInfo(VectorOfNode& peers,
                                std::vector<PubKey>& pubKeys);

  bool CheckUseVCBlockInsteadOfDSBlock(const BlockLink& bl,
                                       VCBlockSharedPtr& prevVCBlockptr);

  std::string GetStateString() const;

  bool VerifyMicroBlockCoSignature(const MicroBlock& microBlock,
                                   uint32_t shardId);

  // DS Reputation functions with no state access.
  static void SaveDSPerformanceCore(
      std::map<uint64_t, std::map<int32_t, std::vector<PubKey>>>&
          coinbaseRewardees,
      std::map<PubKey, uint32_t>& dsMemberPerformance, DequeOfNode& dsComm,
      uint64_t currentEpochNum, unsigned int numOfFinalBlock,
      int finalblockRewardID);
  static unsigned int DetermineByzantineNodesCore(
      unsigned int numOfProposedDSMembers,
      std::vector<PubKey>& removeDSNodePubkeys, uint64_t currentEpochNum,
      unsigned int numOfFinalBlock, double performanceThreshold,
      unsigned int maxByzantineRemoved, DequeOfNode& dsComm,
      const std::map<PubKey, uint32_t>& dsMemberPerformance);

 private:
  static std::map<DirState, std::string> DirStateStrings;

  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
  bool ValidateViewChangeState(DirState NodeState, DirState StatePropose);

  void AddDSPoWs(const PubKey& Pubk, const PoWSolution& DSPOWSoln);
  MapOfPubKeyPoW GetAllDSPoWs();
  void ClearDSPoWSolns();
  std::array<unsigned char, 32> GetDSPoWSoln(const PubKey& Pubk);
  bool IsNodeSubmittedDSPoWSoln(const PubKey& Pubk);
  uint32_t GetNumberOfDSPoWSolns();
  void ClearVCBlockVector();
  bool RunConsensusOnFinalBlockWhenDSPrimary();
  bool CheckIfDSNode(const PubKey& submitterPubKey);
  bool RemoveDSMicroBlock();

  static CoSignatures ConsensusObjectToCoSig(
      const ConsensusCommon& consensusObject);
};

#endif  // ZILLIQA_SRC_LIBDIRECTORYSERVICE_DIRECTORYSERVICE_H_
