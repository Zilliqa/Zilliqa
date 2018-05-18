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

#include "common/Broadcastable.h"
#include "common/Executable.h"
#include "libConsensus/Consensus.h"
#include "libData/BlockData/Block.h"
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
#ifdef STAT_TEST
    std::chrono::system_clock::time_point m_timespec;
#endif // STAT_TEST

    enum Action
    {
        PROCESS_POW1SUBMISSION = 0x00,
        VERIFYPOW1,
        PROCESS_DSBLOCKCONSENSUS,
        PROCESS_POW2SUBMISSION,
        VERIFYPOW2,
        PROCESS_SHARDINGCONSENSUS,
        PROCESS_MICROBLOCKSUBMISSION,
        PROCESS_FINALBLOCKCONSENSUS
    };

    string ActionString(enum Action action)
    {
        switch (action)
        {
        case PROCESS_POW1SUBMISSION:
            return "PROCESS_POW1SUBMISSION";
        case VERIFYPOW1:
            return "VERIFYPOW1";
        case PROCESS_DSBLOCKCONSENSUS:
            return "PROCESS_DSBLOCKCONSENSUS";
        case PROCESS_POW2SUBMISSION:
            return "PROCESS_POW2SUBMISSION";
        case VERIFYPOW2:
            return "VERIFYPOW2";
        case PROCESS_SHARDINGCONSENSUS:
            return "PROCESS_SHARDINGCONSENSUS";
        case PROCESS_MICROBLOCKSUBMISSION:
            return "PROCESS_MICROBLOCKSUBMISSION";
        case PROCESS_FINALBLOCKCONSENSUS:
            return "PROCESS_FINALBLOCKCONSENSUS";
        }
        return "Unknown Action";
    }
    std::atomic<bool> m_requesting_last_ds_block;
    unsigned int BUFFER_TIME_BEFORE_DS_BLOCK_REQUEST = 5;

    // std::shared_timed_mutex m_mutexProducerConsumer;
    std::mutex m_mutexConsensus;

    bool m_hasAllPoWconns = true;
    std::condition_variable cv_allPowConns;
    std::mutex m_MutexCVAllPowConn;

    // Sharding committee members
    std::vector<std::map<PubKey, Peer>> m_shards;
    std::map<PubKey, uint32_t> m_publicKeyToShardIdMap;

    std::mutex m_MutexScheduleFinalBlockConsensus;
    std::condition_variable cv_scheduleFinalBlockConsensus;

    // PoW common variables
    std::mutex m_mutexAllPoWs;
    std::map<PubKey, Peer> m_allPoWConns;
    std::mutex m_mutexAllPoWConns;

    // Consensus variables
    std::shared_ptr<ConsensusCommon> m_consensusObject;
    std::vector<unsigned char> m_consensusBlockHash;

    // PoW1 (DS block) consensus variables
    std::shared_ptr<DSBlock> m_pendingDSBlock;
    std::mutex m_mutexPendingDSBlock;
    std::mutex m_mutexDSBlockConsensus;
    std::vector<std::pair<PubKey, boost::multiprecision::uint256_t>> m_allPoW1s;
    std::mutex m_mutexAllPOW1;

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

    // Recovery (simplified view change)
    std::atomic<unsigned int> m_viewChangeCounter;
    std::atomic<bool> m_initiatedViewChange;
    std::mutex m_mutexProcessViewChangeRequests;
    std::mutex m_mutexRecoveryDSBlockConsensus;
    std::condition_variable cv_RecoveryDSBlockConsensus;
    std::mutex m_mutexRecoveryShardingConsensus;
    std::condition_variable cv_RecoveryShardingConsensus;
    std::mutex m_mutexRecoveryFinalBlockConsensus;
    std::condition_variable cv_RecoveryFinalBlockConsensus;

    const double VC_TOLERANCE_FRACTION = (double)0.667;
    std::atomic<uint64_t> m_viewChangeEpoch;
    std::unordered_map<unsigned int, unsigned int> m_viewChangeRequestTracker;
    std::vector<Peer> m_viewChangeRequesters;
    std::mutex m_mutexViewChangeRequesters;

    std::condition_variable cv_viewChangeDSBlock;
    std::mutex m_MutexCVViewChangeDSBlock;
    std::condition_variable cv_viewChangeSharding;
    std::mutex m_MutexCVViewChangeSharding;
    std::condition_variable cv_viewChangeFinalBlock;
    std::mutex m_MutexCVViewChangeFinalBlock;
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
    std::condition_variable cv_POW1Submission;
    std::mutex m_MutexCVPOW1Submission;
    std::condition_variable cv_POW2Submission;
    std::mutex m_MutexCVPOW2Submission;

    // TO Remove
    //bool temp_todie;

    Mediator& m_mediator;

    Synchronizer m_synchronizer;

    const uint32_t RESHUFFLE_INTERVAL = 500;

    // Message handlers
    bool ProcessSetPrimary(const std::vector<unsigned char>& message,
                           unsigned int offset, const Peer& from);
    bool ProcessPoW1Submission(const std::vector<unsigned char>& message,
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
    bool ProcessAllPoWConnRequest(const vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    bool ProcessAllPoWConnResponse(const vector<unsigned char>& message,
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
    void SendingShardingStructureToShard(
        vector<std::map<PubKey, Peer>>::iterator& p);

    // PoW1 (DS block) consensus functions
    void RunConsensusOnDSBlock(bool isRejoin = false);
    void ComposeDSBlock();

    // internal calls from RunConsensusOnSharding
    void
    SerializeShardingStructure(vector<unsigned char>& sharding_structure) const;
    bool RunConsensusOnShardingWhenDSPrimary();
    bool RunConsensusOnShardingWhenDSBackup();

    // internal calls from ProcessShardingConsensus
    unsigned int
    SerializeEntireShardingStructure(vector<unsigned char>& sharding_message,
                                     unsigned int curr_offset);
    bool SendEntireShardingStructureToLookupNodes();

    // PoW2 (sharding) consensus functions
    void RunConsensusOnSharding();
    void ComputeSharding();

    // internal calls from RunConsensusOnDSBlock
    bool RunConsensusOnDSBlockWhenDSPrimary();
    bool RunConsensusOnDSBlockWhenDSBackup();

    // internal calls from ProcessDSBlockConsensus
    void StoreDSBlockToStorage(); // To further refactor
    bool SendDSBlockToLookupNodes(DSBlock& lastDSBlock, Peer& winnerpeer);
    void
    DetermineNodesToSendDSBlockTo(const Peer& winnerpeer,
                                  unsigned int& my_DS_cluster_num,
                                  unsigned int& my_pow1nodes_cluster_lo,
                                  unsigned int& my_pow1nodes_cluster_hi) const;
    void SendDSBlockToCluster(const Peer& winnerpeer,
                              unsigned int my_pow1nodes_cluster_lo,
                              unsigned int my_pow1nodes_cluster_hi);
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
    bool ParseMessageAndVerifyPOW1(const vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from);
    void AppendSharingSetupToFinalBlockMessage(
        vector<unsigned char>& finalBlockMessage, unsigned int curr_offset);
    bool CheckWhetherDSBlockIsFresh(
        const boost::multiprecision::uint256_t dsblock_num);
    bool CheckWhetherMaxSubmissionsReceived(Peer peer, PubKey key);
    bool VerifyPoW1Submission(const vector<unsigned char>& message,
                              const Peer& from, PubKey& key,
                              unsigned int curr_offset, uint32_t& portNo,
                              uint64_t& nonce, array<unsigned char, 32>& rand1,
                              array<unsigned char, 32>& rand2,
                              unsigned int& difficulty,
                              boost::multiprecision::uint256_t& block_num);
    void ExtractDataFromMicroblocks(
        TxnHash& microblockTrieRoot, std::vector<BlockHash>& microBlockHashes,
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
    void SaveTxnBodySharingAssignment(const vector<unsigned char>& finalblock,
                                      unsigned int& curr_offset);
    bool WaitForTxnBodies();

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
    void RequestAllPoWConn();
    void LastDSBlockRequest();

    bool ProcessLastDSBlockRequest(const vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from);
    bool ProcessLastDSBlockResponse(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);

    // View change
    void InitViewChange();
    bool ProcessInitViewChange(const vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);
    bool ProcessInitViewChangeResponse(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from);

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
        POW1_SUBMISSION = 0x00,
        DSBLOCK_CONSENSUS_PREP,
        DSBLOCK_CONSENSUS,
        POW2_SUBMISSION,
        SHARDING_CONSENSUS_PREP,
        SHARDING_CONSENSUS,
        MICROBLOCK_SUBMISSION,
        FINALBLOCK_CONSENSUS_PREP,
        FINALBLOCK_CONSENSUS,
        ERROR
    };

    uint32_t m_consensusID;
    uint16_t m_consensusLeaderID;

    /// The current role of this Zilliqa instance within the directory service committee.
    std::atomic<Mode> m_mode;

    /// The current internal state of this DirectoryService instance.
    std::atomic<DirState> m_state;

    /// The ID number of this Zilliqa instance for use with consensus operations.
    uint16_t m_consensusMyID;

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
};

#endif // __DIRECTORYSERVICE_H__