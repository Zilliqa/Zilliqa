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
#include <unordered_set>
#include <vector>

#include "common/Broadcastable.h"
#include "common/Constants.h"
#include "common/Executable.h"
#include "depends/common/FixedHash.h"
#include "libConsensus/Consensus.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/BlockHeader/UnavailableMicroBlock.h"
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
        STARTPOW2,
        PROCESS_SHARDING,
        PROCESS_MICROBLOCKCONSENSUS,
        PROCESS_FINALBLOCK,
        PROCESS_TXNBODY,
        NUM_ACTIONS
    };

    enum SUBMITTRANSACTIONTYPE : unsigned char
    {
        TXNSHARING = 0x00,
        MISSINGTXN = 0x01
    };

    enum REJOINTYPE : unsigned char
    {
        ATFINALBLOCK = 0x00,
        ATNEXTROUND = 0x01,
        ATSTATEROOT = 0x02
    };

    Mediator& m_mediator;

    Synchronizer m_synchronizer;

    std::mutex m_mutexConsensus;

    // Sharding information
    std::deque<PubKey> m_myShardMembersPubKeys;
    std::deque<Peer> m_myShardMembersNetworkInfo;
    std::atomic<bool> m_isPrimary;
    std::atomic<bool> m_isMBSender;
    std::atomic<uint32_t> m_myShardID;
    std::atomic<uint32_t> m_numShards;

    // Transaction sharing assignments
    std::atomic<bool> m_txnSharingIAmSender;
    std::atomic<bool> m_txnSharingIAmForwarder;
    std::vector<std::vector<Peer>> m_txnSharingAssignedNodes;

    // DS committee information
    bool m_isDSNode = true;

    // Consensus variables
    std::mutex m_mutexProcessConsensusMessage;
    std::condition_variable cv_processConsensusMessage;
    std::shared_ptr<ConsensusCommon> m_consensusObject;
    std::mutex m_MutexCVMicroblockConsensus;
    std::condition_variable cv_microblockConsensus;
    std::mutex m_MutexCVMicroblockConsensusObject;
    std::condition_variable cv_microblockConsensusObject;

    std::mutex m_MutexCVFBWaitMB;
    std::condition_variable cv_FBWaitMB;

    std::mutex m_mutexCVMicroBlockMissingTxn;
    std::condition_variable cv_MicroBlockMissingTxn;

    // Persistence Retriever
    std::shared_ptr<Retriever> m_retriever;

    std::vector<unsigned char> m_consensusBlockHash;
    std::atomic<uint32_t> m_consensusMyID;
    std::shared_ptr<MicroBlock> m_microblock;
    std::pair<uint64_t, BlockBase> m_lastMicroBlockCoSig;
    std::mutex m_mutexMicroBlock;

    const static uint32_t RECVTXNDELAY_MILLISECONDS = 3000;
    const static unsigned int GOSSIP_RATE = 48;

    // Transactions information
    std::mutex m_mutexCreatedTransactions;
    std::list<Transaction> m_createdTransactions;

    vector<unsigned char> m_txMessage;

    // prefilled transactions sorted by fromAddress
    std::mutex m_mutexPrefilledTxns;
    std::atomic_size_t m_nRemainingPrefilledTxns{0};
    std::unordered_map<Address, std::list<Transaction>> m_prefilledTxns{};

    std::mutex m_mutexSubmittedTransactions;
    std::unordered_map<boost::multiprecision::uint256_t,
                       std::unordered_map<TxnHash, Transaction>>
        m_submittedTransactions;

    std::mutex m_mutexReceivedTransactions;
    std::unordered_map<boost::multiprecision::uint256_t,
                       std::unordered_map<TxnHash, Transaction>>
        m_receivedTransactions;

    uint32_t m_numOfAbsentTxnHashes;

    std::mutex m_mutexCommittedTransactions;
    std::unordered_map<boost::multiprecision::uint256_t, std::list<Transaction>>
        m_committedTransactions;

    std::mutex m_mutexForwardingAssignment;
    std::unordered_map<boost::multiprecision::uint256_t, std::vector<Peer>>
        m_forwardingAssignment;

    uint64_t m_latestForwardBlockNum;
    std::condition_variable m_cvForwardBlockNumSync;
    std::mutex m_mutexForwardBlockNumSync;

    bool CheckState(Action action);

    // To block certain types of incoming message for certain states
    bool ToBlockMessage(unsigned char ins_byte);

#ifndef IS_LOOKUP_NODE
    // internal calls from ProcessStartPoW
    bool ReadVariablesFromStartPoWMessage(
        const vector<unsigned char>& message, unsigned int offset,
        boost::multiprecision::uint256_t& block_num, uint8_t& difficulty,
        array<unsigned char, 32>& rand1, array<unsigned char, 32>& rand2);
#endif // IS_LOOKUP_NODE

    // internal calls from ProcessStartPoW2
    bool ReadVariablesFromStartPoW2Message(
        const vector<unsigned char>& message, unsigned int offset,
        boost::multiprecision::uint256_t& block_num, uint8_t& difficulty,
        array<unsigned char, 32>& rand1, array<unsigned char, 32>& rand2);
#ifndef IS_LOOKUP_NODE
    void SharePoW2WinningResultWithDS(
        const boost::multiprecision::uint256_t& block_num,
        const ethash_mining_result& winning_result) const;
    void StartPoW2MiningAndShareResultWithDS(
        const boost::multiprecision::uint256_t& block_num, uint8_t difficulty,
        const array<unsigned char, 32>& rand1,
        const array<unsigned char, 32>& rand2) const;
    bool ProcessSubmitMissingTxn(const vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
    bool ProcessSubmitTxnSharing(const vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
#endif // IS_LOOKUP_NODE

    // internal call from ProcessSharding
    bool ReadVariablesFromShardingMessage(const vector<unsigned char>& message,
                                          unsigned int& offset);

    // internal calls from ActOnFinalBlock for NODE_FORWARD_ONLY and SEND_AND_FORWARD
    void LoadForwardingAssignmentFromFinalBlock(
        const vector<Peer>& fellowForwarderNodes,
        const boost::multiprecision::uint256_t& blocknum);
    bool FindTxnInSubmittedTxnsList(
        const TxBlock& finalblock,
        const boost::multiprecision::uint256_t& blocknum, uint8_t sharing_mode,
        vector<Transaction>& txns_to_send, const TxnHash& tx_hash);
    bool FindTxnInReceivedTxnsList(
        const TxBlock& finalblock,
        const boost::multiprecision::uint256_t& blocknum, uint8_t sharing_mode,
        vector<Transaction>& txns_to_send, const TxnHash& tx_hash);
    void
    CommitMyShardsMicroBlock(const TxBlock& finalblock,
                             const boost::multiprecision::uint256_t& blocknum,
                             uint8_t sharing_mode,
                             vector<Transaction>& txns_to_send);
    void BroadcastTransactionsToSendingAssignment(
        const boost::multiprecision::uint256_t& blocknum,
        const vector<Peer>& sendingAssignment, const TxnHash& microBlockTxHash,
        vector<Transaction>& txns_to_send) const;

    void BroadcastStateDeltaToSendingAssignment(
        const boost::multiprecision::uint256_t& blocknum,
        const vector<Peer>& sendingAssignment,
        const StateHash& microBlockStateDeltaHash,
        const TxnHash& microBlockTxHash) const;

    bool LoadUnavailableMicroBlockHashes(
        const TxBlock& finalblock,
        const boost::multiprecision::uint256_t& blocknum);

    bool RemoveTxRootHashFromUnavailableMicroBlock(
        const boost::multiprecision::uint256_t& blocknum,
        const TxnHash& txnRootHash, const StateHash& stateDeltaHash);
    bool RemoveStateDeltaHashFromUnavailableMicroBlock(
        const boost::multiprecision::uint256_t& blocknum,
        const StateHash& stateDeltaHash, const TxnHash& txnRootHash);

    bool
    CheckMicroBlockRootHash(const TxBlock& finalBlock,
                            const boost::multiprecision::uint256_t& blocknum);
    bool IsMicroBlockTxRootHashInFinalBlock(
        TxnHash microBlockTxRootHash, StateHash microBlockStateDeltaHash,
        const boost::multiprecision::uint256_t& blocknum,
        bool& isEveryMicroBlockAvailable);
    bool IsMicroBlockStateDeltaHashInFinalBlock(
        StateHash microBlockStateDeltaHash, TxnHash microBlockTxRootHash,
        const boost::multiprecision::uint256_t& blocknum,
        bool& isEveryMicroBlockAvailable);
    bool IsMyShardMicroBlockTxRootHashInFinalBlock(
        const boost::multiprecision::uint256_t& blocknum,
        bool& isEveryMicroBlockAvailable);
    bool IsMyShardMicroBlockStateDeltaHashInFinalBlock(
        const boost::multiprecision::uint256_t& blocknum,
        bool& isEveryMicroBlockAvailable);
    bool IsMyShardMicroBlockInFinalBlock(
        const boost::multiprecision::uint256_t& blocknum);
    bool
    IsMyShardIdInFinalBlock(const boost::multiprecision::uint256_t& blocknum);
    bool
    ReadAuxilliaryInfoFromFinalBlockMsg(const vector<unsigned char>& message,
                                        unsigned int& cur_offset,
                                        uint32_t& shard_id);
    void StoreState();
    // void StoreMicroBlocks();
    void StoreFinalBlock(const TxBlock& txBlock);
    void InitiatePoW();
    void UpdateStateForNextConsensusRound();
    void ScheduleTxnSubmission();
    void ScheduleMicroBlockConsensus();
    void BeginNextConsensusRound();
    void LoadTxnSharingInfo(const vector<unsigned char>& message,
                            unsigned int cur_offset);
    void CallActOnFinalBlockBasedOnSenderForwarderAssgn(uint8_t shard_id);

    // internal calls from ProcessForwardTransaction
    void LoadFwdingAssgnForThisBlockNum(
        const boost::multiprecision::uint256_t& blocknum,
        vector<Peer>& forward_list);
    bool LoadForwardedTxnsAndCheckRoot(
        const vector<unsigned char>& message, unsigned int cur_offset,
        TxnHash& microBlockTxHash, StateHash& microBlockStateDeltaHash,
        vector<Transaction>& txnsInForwardedMessage);
    // vector<TxnHash> & txnHashesInForwardedMessage);
    bool LoadForwardedStateDeltaAndCheckRoot(
        const vector<unsigned char>& message, unsigned int cur_offset,
        StateHash& microBlockStateDeltaHash, TxnHash& microBlockTxHash);
    void CommitForwardedTransactions(
        const vector<Transaction>& txnsInForwardedMessage,
        const boost::multiprecision::uint256_t& blocknum);

    void DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
        const boost::multiprecision::uint256_t& blocknum);
    void LogReceivedFinalBlockDetails(const TxBlock& txblock);

    // internal calls from ProcessDSBlock
    void LogReceivedDSBlockDetails(const DSBlock& dsblock);
    void StoreDSBlockToDisk(const DSBlock& dsblock);
    void UpdateDSCommiteeComposition(const Peer& winnerpeer); //TODO: Refactor

    // Message handlers
    bool ProcessStartPoW(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);
    bool ProcessSharding(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);
    bool ProcessCreateTransaction(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    bool ProcessSubmitTransaction(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    bool ProcessMicroblockConsensus(const std::vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);
    bool ProcessFinalBlock(const std::vector<unsigned char>& message,
                           unsigned int offset, const Peer& from);
    bool ProcessForwardTransaction(const std::vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from);
    bool ProcessCreateTransactionFromLookup(
        const std::vector<unsigned char>& message, unsigned int offset,
        const Peer& from);
    bool ProcessForwardStateDelta(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
    // bool ProcessCreateAccounts(const std::vector<unsigned char> & message, unsigned int offset, const Peer & from);
    bool ProcessDSBlock(const std::vector<unsigned char>& message,
                        unsigned int offset, const Peer& from);
    bool ProcessDoRejoin(const std::vector<unsigned char>& message,
                         unsigned int offset, const Peer& from);

    bool CheckWhetherDSBlockNumIsLatest(
        const boost::multiprecision::uint256_t dsblock_num);
    bool VerifyDSBlockCoSignature(const DSBlock& dsblock);
    bool VerifyFinalBlockCoSignature(const TxBlock& txblock);
    bool CheckStateRoot(const TxBlock& finalblock);

    // View change
    void UpdateDSCommiteeComposition();
    bool VerifyVCBlockCoSignature(const VCBlock& vcblock);
    bool ProcessVCBlock(const vector<unsigned char>& message,
                        unsigned int cur_offset, const Peer& from);

#ifndef IS_LOOKUP_NODE
    // Transaction functions
    void SubmitTransactions();

    bool OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
                           unsigned int offset, const Peer& from);
    bool
    OnCommitFailure(const std::map<unsigned int, std::vector<unsigned char>>&);

    bool RunConsensusOnMicroBlockWhenShardLeader();
    bool RunConsensusOnMicroBlockWhenShardBackup();
    bool RunConsensusOnMicroBlock();
    bool ComposeMicroBlock();
    void SubmitMicroblockToDSCommittee() const;
    bool
    MicroBlockValidator(const std::vector<unsigned char>& sharding_structure,
                        std::vector<unsigned char>& errorMsg);
    bool CheckLegitimacyOfTxnHashes(std::vector<unsigned char>& errorMsg);
    bool CheckBlockTypeIsMicro();
    bool CheckMicroBlockVersion();
    bool CheckMicroBlockTimestamp();
    bool CheckMicroBlockHashes(std::vector<unsigned char>& errorMsg);
    bool CheckMicroBlockTxnRootHash();
    bool CheckMicroBlockStateDeltaHash();
    bool CheckMicroBlockShardID();

    bool ActOnFinalBlock(uint8_t tx_sharing_mode,
                         vector<Peer> my_shard_receivers,
                         const vector<Peer>& fellowForwarderNodes);

    //Coinbase txns
    bool Coinbase(const BlockBase& lastMicroBlock, const TxBlock& lastTxBlock);
    void InitCoinbase();

    // Is Running from New Process
    bool m_fromNewProcess = true;

    bool m_doRejoinAtNextRound = false;
    bool m_doRejoinAtStateRoot = false;
    bool m_doRejoinAtFinalBlock = false;

    void ResetRejoinFlags();

    // Rejoin the network as a shard node in case of failure happens in protocol
    void RejoinAsNormal();
#endif // IS_LOOKUP_NODE

public:
    enum NodeState : unsigned char
    {
        POW_SUBMISSION = 0x00,
        POW2_SUBMISSION,
        TX_SUBMISSION,
        TX_SUBMISSION_BUFFER,
        MICROBLOCK_CONSENSUS_PREP,
        MICROBLOCK_CONSENSUS,
        WAITING_FINALBLOCK,
        ERROR,
        SYNC
    };

    // This process is newly invoked by shell from late node join script
    bool m_runFromLate = false;

    std::condition_variable m_cvAllMicroBlocksRecvd;
    std::mutex m_mutexAllMicroBlocksRecvd;
    bool m_allMicroBlocksRecvd = true;

    std::mutex m_mutexTempCommitted;
    bool m_tempStateDeltaCommitted = true;

    std::condition_variable m_cvNewRoundStarted;
    std::mutex m_mutexNewRoundStarted;
    bool m_newRoundStarted = true;

    std::mutex m_mutexIsEveryMicroBlockAvailable;

    // Transaction body sharing variables
    std::mutex m_mutexUnavailableMicroBlocks;
    std::unordered_map<
        boost::multiprecision::uint256_t,
        std::unordered_map<UnavailableMicroBlock, std::vector<bool>>>
        m_unavailableMicroBlocks;

    uint32_t m_consensusID;

    std::atomic<uint32_t> m_consensusLeaderID;

    /// The current internal state of this Node instance.
    std::atomic<NodeState> m_state;

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
    uint32_t getShardID() { return m_myShardID; };

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
    void EraseCommittedTransactions(uint64_t epochNum)
    {
        m_committedTransactions.erase(epochNum);
    }

    /// Add new block into tx blockchain
    void AddBlock(const TxBlock& block);
#ifndef IS_LOOKUP_NODE

    // Start synchronization with lookup as a shard node
    void StartSynchronization();

    /// Called from DirectoryService during FINALBLOCK processing.
    bool ActOnFinalBlock(uint8_t tx_sharing_mode, const vector<Peer>& nodes);

    /// Performs PoW mining and submission for DirectoryService committee membership.
    bool StartPoW(const boost::multiprecision::uint256_t& block_num,
                  uint8_t difficulty,
                  const std::array<unsigned char, UINT256_SIZE>& rand1,
                  const std::array<unsigned char, UINT256_SIZE>& rand2);

    /// Performs PoW mining and submission for sharding committee membership.
    bool StartPoW2(const boost::multiprecision::uint256_t block_num,
                   uint8_t difficulty, array<unsigned char, 32> rand1,
                   array<unsigned char, 32> rand2);

    /// Call when the normal node be promoted to DS
    void CleanCreatedTransaction();
#endif // IS_LOOKUP_NODE

private:
    static std::map<NodeState, std::string> NodeStateStrings;
    std::string GetStateString() const;
    static std::map<Action, std::string> ActionStrings;
    std::string GetActionString(Action action) const;
};

#endif // __NODE_H__
