/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#ifndef __DIRECTORYSERVICE_H__
#define __DIRECTORYSERVICE_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <condition_variable>
#include <deque>
#include <libCrypto/Sha2.h>
#include <list>
#include <map>
#include <set>
#include <shared_mutex>
#include <vector>

#include "DSBlockMessage.h"
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

/// Implements Directory Service functionality including PoW verification, DS, Tx Block Consensus and sharding management.
class DirectoryService : public Executable, public Broadcastable
{
    std::chrono::system_clock::time_point m_timespec;

    enum Action
    {
        PROCESS_POWSUBMISSION = 0x00,
        VERIFYPOW,
        PROCESS_DSBLOCKCONSENSUS,
        PROCESS_POW2SUBMISSION,
        VERIFYPOW2,
        PROCESS_SHARDINGCONSENSUS,
        PROCESS_MICROBLOCKSUBMISSION,
        PROCESS_FINALBLOCKCONSENSUS,
        PROCESS_VIEWCHANGECONSENSUS
    };

    std::mutex m_mutexConsensus;

    // Sharding committee members
    std::vector<std::map<PubKey, Peer>> m_shards;
    std::map<PubKey, uint32_t> m_publicKeyToShardIdMap;

    // Transaction sharing assignments
    std::vector<unsigned char> m_txnSharingMessage;
    std::vector<Peer> m_DSReceivers;
    std::vector<std::vector<Peer>> m_shardReceivers;
    std::vector<std::vector<Peer>> m_shardSenders;

    std::mutex m_MutexScheduleFinalBlockConsensus;
    std::condition_variable cv_scheduleFinalBlockConsensus;

    // PoW common variables
    std::mutex m_mutexAllPoWs;
    std::map<PubKey, Peer> m_allPoWConns;
    std::mutex m_mutexAllPoWConns;

    // Consensus variables
    std::shared_ptr<ConsensusCommon> m_consensusObject;
    std::vector<unsigned char> m_consensusBlockHash;

    // PoW (DS block) consensus variables
    std::shared_ptr<DSBlock> m_pendingDSBlock;
    std::mutex m_mutexPendingDSBlock;
    std::mutex m_mutexDSBlockConsensus;
    std::vector<std::pair<PubKey, boost::multiprecision::uint256_t>> m_allPoWs;
    std::mutex m_mutexAllPOW;

    // PoW2 (sharding) consensus variables
    std::map<PubKey, boost::multiprecision::uint256_t> m_allPoW2s;
    std::mutex m_mutexAllPOW2;
    std::map<std::array<unsigned char, BLOCK_HASH_SIZE>, PubKey> m_sortedPoW2s;

    // Final block consensus variables
    std::set<MicroBlock> m_microBlocks;
    std::mutex m_mutexMicroBlocks;
    std::shared_ptr<TxBlock> m_finalBlock;
    std::vector<unsigned char> m_finalBlockMessage;
    std::vector<Peer> m_sharingAssignment;

    // View Change
    std::atomic<uint32_t> m_viewChangeCounter;
    Peer m_candidateLeader;
    std::shared_ptr<VCBlock> m_pendingVCBlock;
    std::mutex m_mutexPendingVCBlock;
    std::condition_variable cv_ViewChangeConsensusObj;
    std::mutex m_MutexCVViewChangeConsensusObj;

    std::condition_variable cv_viewChangeDSBlock;
    std::mutex m_MutexCVViewChangeDSBlock;
    std::condition_variable cv_viewChangeSharding;
    std::mutex m_MutexCVViewChangeSharding;
    std::condition_variable cv_viewChangeFinalBlock;
    std::mutex m_MutexCVViewChangeFinalBlock;
    std::condition_variable cv_ViewChangeVCBlock;
    std::mutex m_MutexCVViewChangeVCBlock;

    // Consensus and consensus object
    std::condition_variable cv_DSBlockConsensus;
    std::mutex m_MutexCVDSBlockConsensus;
    std::condition_variable cv_DSBlockConsensusObject;
    std::mutex m_MutexCVDSBlockConsensusObject;
    std::condition_variable cv_shardingConsensus;
    std::mutex m_MutexCVShardingConsensus;
    std::condition_variable cv_shardingConsensusObject;
    std::mutex m_MutexCVShardingConsensusObject;
    std::condition_variable cv_finalBlockConsensusObject;
    std::mutex m_MutexCVFinalBlockConsensusObject;
    std::condition_variable cv_POWSubmission;
    std::mutex m_MutexCVPOWSubmission;
    std::condition_variable cv_POW2Submission;
    std::mutex m_MutexCVPOW2Submission;
    std::mutex m_mutexProcessConsensusMessage;
    std::condition_variable cv_processConsensusMessage;
    // TO Remove
    Mediator& m_mediator;

    Synchronizer m_synchronizer;

    const uint32_t RESHUFFLE_INTERVAL = 500;

    // Message handlers
    bool ProcessSetPrimary(const std::vector<unsigned char>& message,
                           unsigned int offset, const Peer& from);
    bool ProcessPoWSubmission(const std::vector<unsigned char>& message,
                              unsigned int offset, const Peer& from);
    bool ProcessDSBlockConsensus(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
    bool ProcessPoW2Submission(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);
    bool ProcessShardingConsensus(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    bool ProcessMicroblockSubmission(const std::vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from);
    bool ProcessFinalBlockConsensus(const std::vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);
    bool ProcessViewChangeConsensus(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);
    // To block certain types of incoming message for certain states
    bool ToBlockMessage(unsigned char ins_byte);

#ifndef IS_LOOKUP_NODE
    bool CheckState(Action action);
    bool VerifyPOW2(const vector<unsigned char>& message, unsigned int offset,
                    const Peer& from);

    void NotifySelfToStartPOW2(const vector<unsigned char>& message,
                               unsigned int offset);
    void
    SetupMulticastConfigForShardingStructure(unsigned int& my_DS_cluster_num,
                                             unsigned int& my_shards_lo,
                                             unsigned int& my_shards_hi);
    void SendEntireShardingStructureToShardNodes(unsigned int my_shards_lo,
                                                 unsigned int my_shards_hi);

    // PoW (DS block) consensus functions
    void RunConsensusOnDSBlock(bool isRejoin = false);
    void ComposeDSBlock();

    // internal calls from RunConsensusOnSharding
    bool RunConsensusOnShardingWhenDSPrimary();
    bool RunConsensusOnShardingWhenDSBackup();

    // internal calls from ProcessShardingConsensus
    bool SendEntireShardingStructureToLookupNodes();

    // PoW2 (sharding) consensus functions
    void RunConsensusOnSharding();
    void ComputeSharding();
    void ComputeTxnSharingAssignments();

    // internal calls from RunConsensusOnDSBlock
    bool RunConsensusOnDSBlockWhenDSPrimary();
    bool RunConsensusOnDSBlockWhenDSBackup();

    // internal calls from ProcessDSBlockConsensus
    void StoreDSBlockToStorage(); // To further refactor
    bool SendDSBlockToLookupNodes(DSBlock& lastDSBlock, Peer& winnerpeer);
    void
    DetermineNodesToSendDSBlockTo(const Peer& winnerpeer,
                                  unsigned int& my_DS_cluster_num,
                                  unsigned int& my_pownodes_cluster_lo,
                                  unsigned int& my_pownodes_cluster_hi) const;
    void SendDSBlockToCluster(const Peer& winnerpeer,
                              unsigned int my_pownodes_cluster_lo,
                              unsigned int my_pownodes_cluster_hi);
    void UpdateMyDSModeAndConsensusId();
    void UpdateDSCommiteeComposition(const Peer& winnerpeer); //TODO: Refactor

    void ProcessDSBlockConsensusWhenDone(const vector<unsigned char>& message,
                                         unsigned int offset);

    // internal calls from ProcessFinalBlockConsensus
    bool SendFinalBlockToLookupNodes();
    void ProcessFinalBlockConsensusWhenDone();

    void DetermineShardsToSendFinalBlockTo(unsigned int& my_DS_cluster_num,
                                           unsigned int& my_shards_lo,
                                           unsigned int& my_shards_hi) const;
    void SendFinalBlockToShardNodes(unsigned int my_DS_cluster_num,
                                    unsigned int my_shards_lo,
                                    unsigned int my_shards_hi);

    // Final Block functions
    void RunConsensusOnFinalBlock();
    bool RunConsensusOnFinalBlockWhenDSPrimary();
    bool RunConsensusOnFinalBlockWhenDSBackup();
    void ComposeFinalBlockCore();
    vector<unsigned char> ComposeFinalBlockMessage();
    bool ParseMessageAndVerifyPOW(const vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    bool CheckWhetherDSBlockIsFresh(const uint64_t dsblock_num);
    bool VerifyPoWSubmission(const vector<unsigned char>& message,
                             const Peer& from, PubKey& key,
                             unsigned int curr_offset, uint32_t& portNo,
                             uint64_t& nonce, array<unsigned char, 32>& rand1,
                             array<unsigned char, 32>& rand2,
                             unsigned int& difficulty, uint64_t& block_num);
    void ExtractDataFromMicroblocks(
        TxnHash& microblockTxnTrieRoot, StateHash& microblockDeltaTrieRoot,
        std::vector<MicroBlockHashSet>& microblockHashes,
        std::vector<uint32_t>& shardIDs,
        boost::multiprecision::uint256_t& allGasLimit,
        boost::multiprecision::uint256_t& allGasUsed, uint32_t& numTxs,
        std::vector<bool>& isMicroBlockEmpty, uint32_t& numMicroBlocks) const;
    bool VerifyMicroBlockCoSignature(const MicroBlock& microBlock,
                                     uint32_t shardId);

    // FinalBlockValidator functions
    bool CheckFinalBlockValidity();
    bool CheckBlockTypeIsFinal();
    bool CheckFinalBlockVersion();
    bool CheckPreviousFinalBlockHash();
    bool CheckFinalBlockNumber();
    bool CheckFinalBlockTimestamp();
    bool CheckMicroBlockHashes();
    bool CheckMicroBlockHashRoot();
    bool CheckIsMicroBlockEmpty();
    bool CheckStateRoot();
    void LoadUnavailableMicroBlocks();
    void SaveTxnBodySharingAssignment(
        const vector<unsigned char>& sharding_structure,
        unsigned int curr_offset);
    // Redundant code
    // bool WaitForTxnBodies();

    // DS block consensus validator function
    bool DSBlockValidator(const std::vector<unsigned char>& dsblock,
                          std::vector<unsigned char>& errorMsg);

    // Sharding consensus validator function
    bool ShardingValidator(const std::vector<unsigned char>& sharding_structure,
                           std::vector<unsigned char>& errorMsg);

    // Final block consensus validator function
    bool FinalBlockValidator(const std::vector<unsigned char>& finalblock,
                             std::vector<unsigned char>& errorMsg);

    void StoreFinalBlockToDisk();

    // void StoreMicroBlocksToDisk();

    // Used to reconsile view of m_AllPowConn is different.
    void LastDSBlockRequest();

    bool ProcessLastDSBlockRequest(const vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from);
    bool ProcessLastDSBlockResponse(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);

    // View change
    void RunConsensusOnViewChange();
    void ScheduleViewChangeTimeout();
    void ComputeNewCandidateLeader();
    bool ViewChangeValidator(const vector<unsigned char>& vcBlock,
                             std::vector<unsigned char>& errorMsg);
    bool RunConsensusOnViewChangeWhenCandidateLeader();
    bool RunConsensusOnViewChangeWhenNotCandidateLeader();
    void ProcessViewChangeConsensusWhenDone();
    void ProcessNextConsensus(unsigned char viewChangeState);
    void DetermineShardsToSendVCBlockTo(unsigned int& my_DS_cluster_num,
                                        unsigned int& my_shards_lo,
                                        unsigned int& my_shards_hi) const;
    void SendVCBlockToShardNodes(unsigned int my_DS_cluster_num,
                                 unsigned int my_shards_lo,
                                 unsigned int my_shards_hi,
                                 vector<unsigned char>& vcblock_message);

    // Rejoin the network as a DS node in case of failure happens in protocol
    void RejoinAsDS();

    // Reset certain variables to the initial state
    bool CleanVariables();
#endif // IS_LOOKUP_NODE

public:
    enum Mode : unsigned char
    {
        IDLE = 0x00,
        PRIMARY_DS,
        BACKUP_DS
    };

    enum DirState : unsigned char
    {
        POW_SUBMISSION = 0x00,
        DSBLOCK_CONSENSUS_PREP,
        DSBLOCK_CONSENSUS,
        POW2_SUBMISSION,
        SHARDING_CONSENSUS_PREP,
        SHARDING_CONSENSUS,
        MICROBLOCK_SUBMISSION,
        FINALBLOCK_CONSENSUS_PREP,
        FINALBLOCK_CONSENSUS,
        VIEWCHANGE_CONSENSUS_PREP,
        VIEWCHANGE_CONSENSUS,
        ERROR
    };

    uint32_t m_consensusID;
    uint16_t m_consensusLeaderID;

    /// The current role of this Zilliqa instance within the directory service committee.
    std::atomic<Mode> m_mode;

    /// The current internal state of this DirectoryService instance.
    std::atomic<DirState> m_state;

    /// The state (before view change) of this DirectoryService instance.
    std::atomic<DirState> m_viewChangestate;

    /// The ID number of this Zilliqa instance for use with consensus operations.
    uint16_t m_consensusMyID;

    /// The epoch number when DS tries doing Rejoin
    uint64_t m_latestActiveDSBlockNum = 0;

    /// Constructor. Requires mediator reference to access Node and other global members.
    DirectoryService(Mediator& mediator);

    /// Destructor.
    ~DirectoryService();

#ifndef IS_LOOKUP_NODE
    /// Sets the value of m_state.
    void SetState(DirState state);

    /// Start synchronization with lookup as a DS node
    void StartSynchronization();

    /// Implements the GetBroadcastList function inherited from Broadcastable.
    std::vector<Peer> GetBroadcastList(unsigned char ins_type,
                                       const Peer& broadcast_originator);

    /// Launches separate thread to execute sharding consensus after wait_window seconds.
    void ScheduleShardingConsensus(const unsigned int wait_window);

    /// Post processing after the DS node successfully synchronized with the network
    bool FinishRejoinAsDS();
#endif // IS_LOOKUP_NODE

    /// Implements the Execute function inherited from Executable.
    bool Execute(const std::vector<unsigned char>& message, unsigned int offset,
                 const Peer& from);

    /// Notify POW2 submission to DirectoryService::ProcessPoW2Submission()
    void NotifyPOW2Submission() { cv_POW2Submission.notify_all(); }

private:
    static std::map<DirState, std::string> DirStateStrings;
    std::string GetStateString() const;
    static std::map<Action, std::string> ActionStrings;
    std::string GetActionString(Action action) const;
};

#endif // __DIRECTORYSERVICE_H__
