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
#include "libData/AccountData/ForwardedTxnEntry.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/BlockHeader/UnavailableMicroBlock.h"
#include "libData/DataStructures/MultiIndexContainer.h"
#include "libLookup/Synchronizer.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerStore.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"

class Mediator;
class Retriever;

/// Implements PoW submission and sharding node functionality.
class Node : public Executable, public Broadcastable
{
    enum Action
    {
        STARTPOW = 0x00,
        PROCESS_DSBLOCK,
        PROCESS_MICROBLOCKCONSENSUS,
        PROCESS_FINALBLOCK,
        PROCESS_TXNBODY,
        PROCESS_FALLBACKCONSENSUS,
        PROCESS_FALLBACKBLOCK,
        NUM_ACTIONS
    };

    enum SUBMITTRANSACTIONTYPE : unsigned char
    {
        MISSINGTXN = 0x01
    };

    enum REJOINTYPE : unsigned char
    {
        ATFINALBLOCK = 0x00,
        ATNEXTROUND = 0x01,
        ATSTATEROOT = 0x02
    };

    enum LEGITIMACYRESULT : unsigned char
    {
        SUCCESS = 0x00,
        MISSEDTXN,
        WRONGORDER
    };

    Mediator& m_mediator;

    Synchronizer m_synchronizer;

    // DS block information
    std::mutex m_mutexConsensus;

    // Sharding information
    std::atomic<bool> m_isMBSender;
    std::atomic<uint32_t> m_numShards;

    // MicroBlock Sharing assignments
    std::vector<Peer> m_DSMBReceivers;

    // Transaction sharing assignments
    std::atomic<bool> m_txnSharingIAmForwarder;
    std::vector<std::vector<Peer>> m_txnSharingAssignedNodes;

    // Consensus variables
    std::mutex m_mutexProcessConsensusMessage;
    std::condition_variable cv_processConsensusMessage;
    std::shared_ptr<ConsensusCommon> m_consensusObject;
    std::mutex m_MutexCVMicroblockConsensus;
    std::mutex m_MutexCVMicroblockConsensusObject;
    std::condition_variable cv_microblockConsensusObject;

    std::mutex m_MutexCVFBWaitMB;
    std::condition_variable cv_FBWaitMB;

    std::mutex m_mutexCVMicroBlockMissingTxn;
    std::condition_variable cv_MicroBlockMissingTxn;

    // Persistence Retriever
    std::shared_ptr<Retriever> m_retriever;

    std::vector<unsigned char> m_consensusBlockHash;
    std::shared_ptr<MicroBlock> m_microblock;
    std::pair<uint64_t, BlockBase> m_lastMicroBlockCoSig;
    std::mutex m_mutexMicroBlock;

    const static uint32_t RECVTXNDELAY_MILLISECONDS = 3000;
    const static unsigned int GOSSIP_RATE = 48;

    // Transactions information
    std::mutex m_mutexCreatedTransactions;
    gas_txnid_comp_txns m_createdTransactions;

    std::unordered_map<Address,
                       std::map<boost::multiprecision::uint256_t, Transaction>>
        m_addrNonceTxnMap;
    std::vector<TxnHash> m_txnsOrdering;

    std::mutex m_mutexProcessedTransactions;
    std::unordered_map<uint64_t,
                       std::unordered_map<TxnHash, TransactionWithReceipt>>
        m_processedTransactions;
    //operates under m_mutexProcessedTransaction
    std::vector<TxnHash> m_TxnOrder;

    uint32_t m_numOfAbsentTxnHashes;

    // std::mutex m_mutexCommittedTransactions;
    // std::unordered_map<uint64_t, std::list<TransactionWithReceipt>>
    //     m_committedTransactions;

    std::mutex m_mutexForwardedTxnBuffer;
    std::unordered_map<uint64_t, std::vector<ForwardedTxnEntry>>
        m_forwardedTxnBuffer;

    std::mutex m_mutexTxnPacketBuffer;
    std::unordered_map<uint64_t, std::vector<unsigned char>> m_txnPacketBuffer;

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

    // internal calls from ActOnFinalBlock for NODE_FORWARD_ONLY and SEND_AND_FORWARD
    void LoadForwardingAssignmentFromFinalBlock(
        const std::vector<Peer>& fellowForwarderNodes,
        const uint64_t& blocknum);

    bool FindTxnInProcessedTxnsList(
        const uint64_t& blockNum, uint8_t sharing_mode,
        std::vector<TransactionWithReceipt>& txns_to_send,
        const TxnHash& tx_hash);

    void
    GetMyShardsMicroBlock(const uint64_t& blocknum, uint8_t sharing_mode,
                          std::vector<TransactionWithReceipt>& txns_to_send);

    void BroadcastTransactionsToLookup(
        const std::vector<TransactionWithReceipt>& txns_to_send);

    bool LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                         const uint64_t& blocknum,
                                         bool& toSendTxnToLookup);

    bool ProcessStateDeltaFromFinalBlock(
        const std::vector<unsigned char>& stateDeltaBytes,
        const StateHash& finalBlockStateDeltaHash);

    // internal calls from ProcessForwardTransaction
    void CommitForwardedTransactions(const ForwardedTxnEntry& entry);

    bool
    RemoveTxRootHashFromUnavailableMicroBlock(const ForwardedTxnEntry& entry);

    bool IsMicroBlockTxRootHashInFinalBlock(const ForwardedTxnEntry& entry,
                                            bool& isEveryMicroBlockAvailable);

    bool CheckMicroBlockRootHash(const TxBlock& finalBlock,
                                 const uint64_t& blocknum);

    void StoreState();
    // void StoreMicroBlocks();
    void StoreFinalBlock(const TxBlock& txBlock);
    void InitiatePoW();
    void ScheduleMicroBlockConsensus();
    void BeginNextConsensusRound();

    void CommitMicroBlockConsensusBuffer();

    void
    DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(const uint64_t& blocknum);
    void LogReceivedFinalBlockDetails(const TxBlock& txblock);

    // internal calls from ProcessDSBlock
    void LogReceivedDSBlockDetails(const DSBlock& dsblock);
    void StoreDSBlockToDisk(const DSBlock& dsblock);
    void UpdateDSCommiteeComposition();

    // Message handlers
    bool ProcessStartPoW(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);
    bool ProcessSharding(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);
    bool ProcessSubmitTransaction(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    bool ProcessMicroblockConsensus(const std::vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);
    bool
    ProcessMicroblockConsensusCore(const std::vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from);
    bool ProcessFinalBlock(const std::vector<unsigned char>& message,
                           unsigned int offset, const Peer& from);
    bool ProcessForwardTransaction(const std::vector<unsigned char>& message,
                                   unsigned int cur_offset, const Peer& from);
    bool ProcessForwardTransactionCore(const ForwardedTxnEntry& entry);
    bool ProcessCreateTransactionFromLookup(
        const std::vector<unsigned char>& message, unsigned int offset,
        const Peer& from);
    bool ProcessTxnPacketFromLookup(const std::vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);
    bool ProcessTxnPacketFromLookupCore(
        const std::vector<unsigned char>& message, const uint32_t shardId,
        const std::vector<Transaction>& transactions);

#ifdef HEARTBEAT_TEST
    bool ProcessKillPulse(const std::vector<unsigned char>& message,
                          unsigned int offset, const Peer& from);
#endif // HEARTBEAT_TEST

    // bool ProcessCreateAccounts(const std::vector<unsigned char> & message, unsigned int offset, const Peer & from);
    bool ProcessDSBlock(const std::vector<unsigned char>& message,
                        unsigned int cur_offset, const Peer& from);
    bool ProcessDoRejoin(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);

    bool CheckWhetherDSBlockNumIsLatest(const uint64_t dsblockNum);
    bool VerifyDSBlockCoSignature(const DSBlock& dsblock);
    bool VerifyFinalBlockCoSignature(const TxBlock& txblock);
    bool CheckStateRoot(const TxBlock& finalBlock);

    // View change
    void UpdateDSCommiteeCompositionAfterVC();
    bool VerifyVCBlockCoSignature(const VCBlock& vcblock);
    bool ProcessVCBlock(const std::vector<unsigned char>& message,
                        unsigned int cur_offset, const Peer& from);

    // Transaction functions
    bool OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
                           const Peer& from);
    bool
    OnCommitFailure(const std::map<unsigned int, std::vector<unsigned char>>&);

    bool RunConsensusOnMicroBlockWhenShardLeader();
    bool RunConsensusOnMicroBlockWhenShardBackup();
    bool ComposeMicroBlock();
    void SubmitMicroblockToDSCommittee() const;
    bool MicroBlockValidator(const std::vector<unsigned char>& message,
                             unsigned int offset,
                             std::vector<unsigned char>& errorMsg,
                             const uint32_t consensusID,
                             const uint64_t blockNumber,
                             const std::vector<unsigned char>& blockHash,
                             const uint16_t leaderID, const PubKey& leaderKey,
                             std::vector<unsigned char>& messageToCosign);
    unsigned char
    CheckLegitimacyOfTxnHashes(std::vector<unsigned char>& errorMsg);
    bool CheckBlockTypeIsMicro();
    bool CheckMicroBlockVersion();
    bool CheckMicroBlockshardId();
    bool CheckMicroBlockTimestamp();
    bool CheckMicroBlockHashes(std::vector<unsigned char>& errorMsg);
    bool CheckMicroBlockTxnRootHash();
    bool CheckMicroBlockStateDeltaHash();
    bool CheckMicroBlockTranReceiptHash();

    void BroadcastMicroBlockToLookup();
    bool VerifyTxnsOrdering(const std::vector<TxnHash>& tranHashes,
                            std::list<Transaction>& curTxns);

    void ProcessTransactionWhenShardLeader();
    bool
    ProcessTransactionWhenShardBackup(const std::vector<TxnHash>& tranHashes,
                                      std::vector<TxnHash>& missingtranHashes);

    // Fallback Consensus
    void FallbackTimerLaunch();
    void FallbackTimerPulse();
    bool FallbackValidator(const std::vector<unsigned char>& message,
                           unsigned int offset,
                           std::vector<unsigned char>& errorMsg,
                           const uint32_t consensusID,
                           const uint64_t blockNumber,
                           const std::vector<unsigned char>& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           std::vector<unsigned char>& messageToCosign);
    void UpdateFallbackConsensusLeader();
    void SetLastKnownGoodState();
    void ComposeFallbackBlock();
    void RunConsensusOnFallback();
    bool RunConsensusOnFallbackWhenLeader();
    bool RunConsensusOnFallbackWhenBackup();
    void ProcessFallbackConsensusWhenDone();
    bool ProcessFallbackConsensus(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    // Fallback block processing
    void UpdateDSCommittee(const uint32_t& shard_id, const PubKey& leaderPubKey,
                           const Peer& leaderNetworkInfo);
    bool VerifyFallbackBlockCoSignature(const FallbackBlock& fallbackblock);
    bool ProcessFallbackBlock(const std::vector<unsigned char>& message,
                              unsigned int cur_offset, const Peer& from);

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
        uint32_t cluster_size, uint32_t num_of_child_clusters,
        uint32_t& nodes_lo, uint32_t& nodes_hi);

public:
    enum NodeState : unsigned char
    {
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

    // This process is newly invoked by shell from late node join script
    bool m_runFromLate = false;

    // std::condition_variable m_cvAllMicroBlocksRecvd;
    // std::mutex m_mutexAllMicroBlocksRecvd;
    // bool m_allMicroBlocksRecvd = true;

    std::shared_ptr<std::deque<std::pair<PubKey, Peer>>> m_myShardMembers;

    // std::condition_variable m_cvNewRoundStarted;
    // std::mutex m_mutexNewRoundStarted;
    // bool m_newRoundStarted = false;

    std::mutex m_mutexIsEveryMicroBlockAvailable;

    // Transaction sharing assignment
    std::atomic<bool> m_txnSharingIAmSender;

    // Transaction body sharing variables
    std::mutex m_mutexUnavailableMicroBlocks;
    std::unordered_map<uint64_t, std::vector<UnavailableMicroBlock>>
        m_unavailableMicroBlocks;

    /// Sharding variables
    std::atomic<uint32_t> m_myshardId;
    std::atomic<uint32_t> m_consensusMyID;
    std::atomic<bool> m_isPrimary;
    std::atomic<uint32_t> m_consensusLeaderID;

    // Finalblock Processing
    std::mutex m_mutexFinalBlock;

    // DS block information
    std::mutex m_mutexDSBlock;

    /// The current internal state of this Node instance.
    std::atomic<NodeState> m_state;

    // a buffer flag used by lookup to store the isVacuousEpoch state before StoreFinalBlock
    std::atomic<bool> m_isVacuousEpochBuffer;

    // a indicator of whether recovered from fallback just now
    bool m_justDidFallback = false;

    /// Constructor. Requires mediator reference to access DirectoryService and other global members.
    Node(Mediator& mediator, unsigned int syncType, bool toRetrieveHistory);

    /// Destructor.
    ~Node();

    /// Install the Node
    void Install(unsigned int syncType, bool toRetrieveHistory = true);

    /// Set initial state, variables, and clean-up storage
    void Init();

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
    bool StartRetrieveHistory();

    //Erase m_committedTransactions for given epoch number
    // void EraseCommittedTransactions(uint64_t epochNum)
    // {
    //     std::lock_guard<std::mutex> g(m_mutexCommittedTransactions);
    //     m_committedTransactions.erase(epochNum);
    // }

    /// Add new block into tx blockchain
    void AddBlock(const TxBlock& block);

    void CommitForwardedTransactionBuffer();

    void CleanCreatedTransaction();

    void CleanMicroblockConsensusBuffer();

    void CallActOnFinalblock();

    void UpdateStateForNextConsensusRound();

    // Start synchronization with lookup as a shard node
    void StartSynchronization();

    /// Performs PoW mining and submission for DirectoryService committee membership.
    bool StartPoW(const uint64_t& block_num, uint8_t ds_difficulty,
                  uint8_t difficulty,
                  const std::array<unsigned char, UINT256_SIZE>& rand1,
                  const std::array<unsigned char, UINT256_SIZE>& rand2);

    /// Send PoW soln to DS Commitee
    bool SendPoWResultToDSComm(const uint64_t& block_num,
                               const uint8_t& difficultyLevel,
                               const uint64_t winningNonce,
                               const std::string& powResultHash,
                               const std::string& powMixhash);

    /// Used by oldest DS node to configure shard ID as a new shard node
    void SetMyshardId(uint32_t shardId);

    /// Used by oldest DS node to finish setup as a new shard node
    void StartFirstTxEpoch();

    /// Used for start consensus on microblock
    bool RunConsensusOnMicroBlock();

    /// Used for commit buffered txn packet
    void CommitTxnPacketBuffer();

    /// Used by oldest DS node to configure sharding variables as a new shard node
    bool LoadShardingStructure();

    /// Used by oldest DS node to configure txn sharing assignments as a new shard node

    void LoadTxnSharingInfo();

    // Rejoin the network as a shard node in case of failure happens in protocol
    void RejoinAsNormal();

    /// Force state changes from MBCON/MBCON_PREP -> WAITING_FINALBLOCK
    void PrepareGoodStateForFinalBlock();

    /// Reset Consensus ID
    void ResetConsensusId();

private:
    static std::map<NodeState, std::string> NodeStateStrings;
    std::string GetStateString() const;
    static std::map<Action, std::string> ActionStrings;
    std::string GetActionString(Action action) const;
    /// Fallback Consensus Related
    std::atomic<NodeState> m_fallbackState;
    bool ValidateFallbackState(NodeState nodeState, NodeState statePropose);
};

#endif // __NODE_H__
