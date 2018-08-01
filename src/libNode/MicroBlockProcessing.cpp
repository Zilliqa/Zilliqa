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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnRootComputation.h"
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void Node::SubmitMicroblockToDSCommittee() const
{
    // Message = [8-byte DS blocknum] [4-byte consensusid] [4-byte shard ID] [Tx microblock]
    vector<unsigned char> microblock
        = {MessageType::DIRECTORY, DSInstructionType::MICROBLOCKSUBMISSION};
    unsigned int cur_offset = MessageOffset::BODY;

    // 8-byte DS blocknum
    uint64_t DSBlockNum
        = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    Serializable::SetNumber<uint64_t>(microblock, cur_offset, DSBlockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    // 4-byte consensusid
    Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_consensusID,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // // 4-byte shard ID
    // Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_myShardID,
    //                                   sizeof(uint32_t));
    // cur_offset += sizeof(uint32_t);

    // Tx microblock
    m_microblock->SerializeCore(microblock, cur_offset);

    LOG_STATE("[MICRO][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] SENT");
    deque<Peer> peerList;

    for (auto const& i : m_mediator.m_DSCommittee)
    {
        peerList.push_back(i.second);
    }

    P2PComm::GetInstance().SendBroadcastMessage(peerList, microblock);
}

void Node::LoadForwardingAssignment(const vector<Peer>& fellowForwarderNodes,
                                    const uint64_t& blocknum)
{
    // For now, since each sharding setup only processes one block, then whatever transactions we
    // failed to submit have to be discarded m_createdTransactions.clear();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[shard " << m_myShardID
                        << "] I am a forwarder for transactions in block "
                        << blocknum);

    lock_guard<mutex> g2(m_mutexForwardingAssignment);

    m_forwardingAssignment.emplace(blocknum, vector<Peer>());
    vector<Peer>& peers = m_forwardingAssignment.at(blocknum);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forward list:");

    for (unsigned int i = 0; i < m_myShardMembersNetworkInfo.size(); i++)
    {
        if (i == m_consensusMyID)
        {
            continue;
        }
        // if (rand() % m_myShardMembersNetworkInfo.size() <= GOSSIP_RATE)
        // {
        //     peers.emplace_back(m_myShardMembersNetworkInfo.at(i));
        // }
        peers.emplace_back(m_myShardMembersNetworkInfo.at(i));
    }

    for (unsigned int i = 0; i < fellowForwarderNodes.size(); i++)
    {
        Peer fellowforwarder = fellowForwarderNodes[i];

        for (unsigned int j = 0; j < peers.size(); j++)
        {
            if (peers.at(j) == fellowforwarder)
            {
                peers.at(j) = move(peers.back());
                peers.pop_back();
                break;
            }
        }
    }

    for (unsigned int i = 0; i < peers.size(); i++)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  peers.at(i));
    }
}

void Node::ActOnMicroBlockDone(uint8_t tx_sharing_mode,
                               const vector<Peer>& nodes)
{
    // If tx_sharing_mode=IDLE              ==> Body = [ignored]
    // If tx_sharing_mode=SEND_ONLY         ==> Body = [num receivers in other shards] [IP and node] ... [IP and node]
    // If tx_sharing_mode=DS_FORWARD_ONLY   ==> Body = [num receivers in DS comm] [IP and node] ... [IP and node]
    // If tx_sharing_mode=NODE_FORWARD_ONLY ==> Body = [num fellow forwarders] [IP and node] ... [IP and node]
    LOG_MARKER();

    const uint64_t& blocknum = m_mediator.m_currentEpochNum;

    vector<Peer> sendingAssignment;

    switch (tx_sharing_mode)
    {
    case SEND_ONLY:
    {
        sendingAssignment = nodes;
        break;
    }
    case DS_FORWARD_ONLY:
    {
        lock_guard<mutex> g2(m_mutexForwardingAssignment);
        m_forwardingAssignment.emplace(blocknum, nodes);
        break;
    }
    case NODE_FORWARD_ONLY:
    {
        LoadForwardingAssignment(nodes, blocknum);
        break;
    }
    case IDLE:
    default:
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am idle for transactions in block " << blocknum);
        break;
    }
    }

    vector<Transaction> txns_to_send;

    GetMyShardsMicroBlock(blocknum, tx_sharing_mode, txns_to_send);

    if (sendingAssignment.size() > 0)
    {
        BroadcastTransactionsToSendingAssignment(
            blocknum, sendingAssignment,
            m_microblock->GetHeader().GetTxRootHash(), txns_to_send);
    }
}

void Node::ActOnMicroBlockDone(uint8_t tx_sharing_mode,
                               vector<Peer> sendingAssignment,
                               const vector<Peer>& fellowForwarderNodes)
{
    // Body = [num receivers in  other shards] [IP and node] ... [IP and node]
    //        [num fellow forwarders] [IP and node] ... [IP and node]

    LOG_MARKER();

    if (tx_sharing_mode == SEND_AND_FORWARD)
    {
        const uint64_t& blocknum = m_mediator.m_currentEpochNum;

        LoadForwardingAssignment(fellowForwarderNodes, blocknum);

        vector<Transaction> txns_to_send;

        GetMyShardsMicroBlock(blocknum, tx_sharing_mode, txns_to_send);

        if (sendingAssignment.size() > 0)
        {
            BroadcastTransactionsToSendingAssignment(
                blocknum, sendingAssignment,
                m_microblock->GetHeader().GetTxRootHash(), txns_to_send);
        }
    }
}

void Node::GetMyShardsMicroBlock(const uint64_t& blocknum, uint8_t sharing_mode,
                                 vector<Transaction>& txns_to_send)
{
    LOG_MARKER();

    const vector<TxnHash>& tx_hashes = m_microblock->GetTranHashes();
    for (unsigned i = 0; i < tx_hashes.size(); i++)
    {
        const TxnHash& tx_hash = tx_hashes.at(i);

        if (FindTxnInSubmittedTxnsList(blocknum, sharing_mode, txns_to_send,
                                       tx_hash))
        {
            continue;
        }

        if (!FindTxnInReceivedTxnsList(blocknum, sharing_mode, txns_to_send,
                                       tx_hash))
        {
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Failed trying to find txn in submitted txn and recv list");
        }
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of transactions to broadcast for block "
                  << blocknum << " = " << txns_to_send.size());

    {
        lock_guard<mutex> g(m_mutexReceivedTransactions);
        m_receivedTransactions.erase(blocknum);
    }
    {
        lock_guard<mutex> g2(m_mutexSubmittedTransactions);
        m_submittedTransactions.erase(blocknum);
    }
}

void Node::BroadcastTransactionsToSendingAssignment(
    const uint64_t& blocknum, const vector<Peer>& sendingAssignment,
    const TxnHash& microBlockTxHash, vector<Transaction>& txns_to_send) const
{
    LOG_MARKER();

    LOG_STATE(
        "[TXBOD]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] BEFORE TXN BODIES #" << blocknum);

    if (txns_to_send.size() > 0)
    {
        // Transaction body sharing
        unsigned int cur_offset = MessageOffset::BODY;
        vector<unsigned char> forwardtxn_message
            = {MessageType::NODE, NodeInstructionType::FORWARDTRANSACTION};

        // block num
        Serializable::SetNumber<uint64_t>(forwardtxn_message, cur_offset,
                                          blocknum, sizeof(uint64_t));
        cur_offset += sizeof(uint64_t);

        // microblock tx hash
        copy(microBlockTxHash.asArray().begin(),
             microBlockTxHash.asArray().end(),
             back_inserter(forwardtxn_message));
        cur_offset += TRAN_HASH_SIZE;

        // microblock state delta hash
        StateHash microBlockDeltaHash
            = m_microblock->GetHeader().GetStateDeltaHash();
        copy(microBlockDeltaHash.asArray().begin(),
             microBlockDeltaHash.asArray().end(),
             back_inserter(forwardtxn_message));
        cur_offset += STATE_HASH_SIZE;

        for (unsigned int i = 0; i < txns_to_send.size(); i++)
        {
            // txn body
            txns_to_send.at(i).Serialize(forwardtxn_message, cur_offset);
            cur_offset += txns_to_send.at(i).GetSerializedSize();

            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //           "[TXN] ["
            //               << blocknum << "] Broadcasted   = 0x"
            //               << DataConversion::charArrToHexStr(
            //                      txns_to_send.at(i).GetTranID().asArray()));
        }

        // P2PComm::GetInstance().SendBroadcastMessage(sendingAssignment,
        //                                             forwardtxn_message);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG: I have broadcasted the txn body!")

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I will soon be sending the txn bodies to the lookup nodes");
        m_mediator.m_lookup->SendMessageToLookupNodes(forwardtxn_message);
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG I have no txn body to send")
    }

    if (m_microblock->GetHeader().GetStateDeltaHash() != StateHash())
    {

        BroadcastStateDeltaToSendingAssignment(
            blocknum, sendingAssignment,
            m_microblock->GetHeader().GetStateDeltaHash(), microBlockTxHash);
    }

    LOG_STATE(
        "[TXBOD]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] AFTER SENDING TXN BODIES");
}

void Node::BroadcastStateDeltaToSendingAssignment(
    const uint64_t& blocknum, const vector<Peer>& sendingAssignment,
    const StateHash& microBlockStateDeltaHash,
    const TxnHash& microBlockTxHash) const
{
    LOG_MARKER();

    unsigned int cur_offset = MessageOffset::BODY;
    vector<unsigned char> forwardstate_message
        = {MessageType::NODE, NodeInstructionType::FORWARDSTATEDELTA};

    // block num
    Serializable::SetNumber<uint64_t>(forwardstate_message, cur_offset,
                                      blocknum, sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    // microblock state delta hash
    copy(microBlockStateDeltaHash.asArray().begin(),
         microBlockStateDeltaHash.asArray().end(),
         back_inserter(forwardstate_message));
    cur_offset += STATE_HASH_SIZE;

    // microblock tx hash
    copy(microBlockTxHash.asArray().begin(), microBlockTxHash.asArray().end(),
         back_inserter(forwardstate_message));
    cur_offset += TRAN_HASH_SIZE;

    // state delta
    vector<unsigned char> stateDel;
    AccountStore::GetInstance().GetSerializedDelta(stateDel);

    copy(stateDel.begin(), stateDel.end(), back_inserter(forwardstate_message));

    P2PComm::GetInstance().SendBroadcastMessage(sendingAssignment,
                                                forwardstate_message);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Broadcasted the state delta! ");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Sending the state delta to the lookup nodes");
    m_mediator.m_lookup->SendMessageToLookupNodes(forwardstate_message);
}

bool Node::FindTxnInSubmittedTxnsList(const uint64_t& blockNum,
                                      uint8_t sharing_mode,
                                      vector<Transaction>& txns_to_send,
                                      const TxnHash& tx_hash)
{
    lock(m_mutexSubmittedTransactions, m_mutexCommittedTransactions);
    lock_guard<mutex> g(m_mutexSubmittedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexCommittedTransactions, adopt_lock);

    auto& submittedTransactions = m_submittedTransactions[blockNum];
    auto& committedTransactions = m_committedTransactions[blockNum];
    const auto& txnIt = submittedTransactions.find(tx_hash);

    // Check if transaction is part of submitted Tx list
    if (txnIt != submittedTransactions.end())
    {
        if ((sharing_mode == SEND_ONLY) || (sharing_mode == SEND_AND_FORWARD))
        {
            txns_to_send.emplace_back(txnIt->second);
        }

        // Move entry from submitted Tx list to committed Tx list
        committedTransactions.emplace_back(txnIt->second);
        submittedTransactions.erase(txnIt);

        return true;
    }

    return false;
}

bool Node::FindTxnInReceivedTxnsList(const uint64_t& blockNum,
                                     uint8_t sharing_mode,
                                     vector<Transaction>& txns_to_send,
                                     const TxnHash& tx_hash)
{
    lock(m_mutexReceivedTransactions, m_mutexCommittedTransactions);
    lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexCommittedTransactions, adopt_lock);

    auto& receivedTransactions = m_receivedTransactions[blockNum];
    auto& committedTransactions = m_committedTransactions[blockNum];
    const auto& txnIt = receivedTransactions.find(tx_hash);

    // Check if transaction is part of received Tx list
    if (txnIt != receivedTransactions.end())
    {
        if ((sharing_mode == SEND_ONLY) || (sharing_mode == SEND_AND_FORWARD))
        {
            txns_to_send.emplace_back(txnIt->second);
        }

        // Move entry from received Tx list to committed Tx list
        committedTransactions.emplace_back(txnIt->second);
        receivedTransactions.erase(txnIt);

        return true;
    }

    return false;
}

void Node::CallActOnMicroblockDoneBasedOnSenderForwarderAssign(uint8_t shard_id)
{
    if ((m_txnSharingIAmSender == false) && (m_txnSharingIAmForwarder == true))
    {
        // Give myself the list of my fellow forwarders
        const vector<Peer>& my_shard_receivers
            = m_txnSharingAssignedNodes.at(shard_id + 1);
        ActOnMicroBlockDone(TxSharingMode::NODE_FORWARD_ONLY,
                            my_shard_receivers);
    }
    else if ((m_txnSharingIAmSender == true)
             && (m_txnSharingIAmForwarder == false))
    {
        vector<Peer> nodes_to_send;

        // Give myself the list of all receiving nodes in all other committees including DS
        for (unsigned int i = 0; i < m_txnSharingAssignedNodes.at(0).size();
             i++)
        {
            nodes_to_send.emplace_back(m_txnSharingAssignedNodes[0][i]);
        }

        for (unsigned int i = 1; i < m_txnSharingAssignedNodes.size(); i += 2)
        {
            if (((i - 1) / 2) == shard_id)
            {
                continue;
            }

            const vector<Peer>& shard = m_txnSharingAssignedNodes.at(i);
            for (unsigned int j = 0; j < shard.size(); j++)
            {
                nodes_to_send.emplace_back(shard[j]);
            }
        }

        ActOnMicroBlockDone(TxSharingMode::SEND_ONLY, nodes_to_send);
    }
    else if ((m_txnSharingIAmSender == true)
             && (m_txnSharingIAmForwarder == true))
    {
        // Give myself the list of my fellow forwarders
        const vector<Peer>& my_shard_receivers
            = m_txnSharingAssignedNodes.at(shard_id + 1);

        vector<Peer> fellowForwarderNodes;

        // Give myself the list of all receiving nodes in all other committees including DS
        for (unsigned int i = 0; i < m_txnSharingAssignedNodes.at(0).size();
             i++)
        {
            fellowForwarderNodes.emplace_back(m_txnSharingAssignedNodes[0][i]);
        }

        for (unsigned int i = 1; i < m_txnSharingAssignedNodes.size(); i += 2)
        {
            if (((i - 1) / 2) == shard_id)
            {
                continue;
            }

            const vector<Peer>& shard = m_txnSharingAssignedNodes.at(i);
            for (unsigned int j = 0; j < shard.size(); j++)
            {
                fellowForwarderNodes.emplace_back(shard[j]);
            }
        }

        ActOnMicroBlockDone(TxSharingMode::SEND_AND_FORWARD,
                            fellowForwarderNodes, my_shard_receivers);
    }
    else
    {
        ActOnMicroBlockDone(TxSharingMode::IDLE, vector<Peer>());
    }
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessMicroblockConsensus(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();

    {
        lock_guard<mutex> g(m_mutexConsensus);

        // Consensus messages must be processed in correct sequence as they come in
        // It is possible for ANNOUNCE to arrive before correct DS state
        // In that case, state transition will occurs and ANNOUNCE will be processed.

        if ((m_state == TX_SUBMISSION) || (m_state == TX_SUBMISSION_BUFFER)
            || (m_state == MICROBLOCK_CONSENSUS_PREP))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Received microblock announcement from shard leader. I "
                      "will move on to consensus");
            cv_microblockConsensus.notify_all();

            std::unique_lock<std::mutex> cv_lk(
                m_MutexCVMicroblockConsensusObject);

            if (cv_microblockConsensusObject.wait_for(
                    cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
                    [this] { return (m_state == MICROBLOCK_CONSENSUS); }))
            {
                // condition passed without timeout
            }
            else
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Time out while waiting for state transition and "
                          "consensus object creation ");
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "State transition is completed and consensus object "
                      "creation.");
        }
    }

    if (!CheckState(PROCESS_MICROBLOCKCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in MICROBLOCK_CONSENSUS state");
        return false;
    }

    // Consensus message must be processed in order. The following will block till it is the right order.

    std::unique_lock<mutex> cv_lk(m_mutexProcessConsensusMessage);
    if (cv_processConsensusMessage.wait_for(
            cv_lk, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
            [this, message, offset]() -> bool {
                lock_guard<mutex> g(m_mutexConsensus);
                if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
                {
                    LOG_GENERAL(WARNING,
                                "The node started the process of rejoining, "
                                "Ignore rest of "
                                "consensus msg.")
                    return false;
                }

                if (m_consensusObject == nullptr)
                {
                    LOG_GENERAL(WARNING,
                                "m_consensusObject should have been created "
                                "but it is not")
                    return false;
                }
                return m_consensusObject->CanProcessMessage(message, offset);
            }))
    {
        // Correct order preserved
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Timeout while waiting for correct order of consensus "
                    "messages");
        return false;
    }

    lock_guard<mutex> g(m_mutexConsensus);

    if (!m_consensusObject->ProcessMessage(message, offset, from))
    {
        return false;
    }

    ConsensusCommon::State state = m_consensusObject->GetState();

    if (state == ConsensusCommon::State::DONE)
    {
        if (m_isPrimary == true)
        {
            LOG_STATE("[MICON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_currentEpochNum << "] DONE");

            // Update the micro block with the co-signatures from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        if (m_isMBSender == true)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Designated as Microblock sender");

            // Update the micro block with the co-signature from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        SetState(WAITING_FINALBLOCK);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Micro block consensus "
                      << "is DONE!!! (Epoch " << m_mediator.m_currentEpochNum
                      << ")");
        m_lastMicroBlockCoSig.first = m_mediator.m_currentEpochNum;
        m_lastMicroBlockCoSig.second.SetCoSignatures(*m_consensusObject);

        CallActOnMicroblockDoneBasedOnSenderForwarderAssign(
            m_microblock->GetHeader().GetShardID());

        lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
        cv_FBWaitMB.notify_all();
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");

        if (m_consensusObject->GetConsensusErrorCode()
            == ConsensusCommon::MISSING_TXN)
        {
            // Missing txns in microblock proposed by leader. Will attempt to fetch
            // missing txns from leader, set to a valid state to accept cosig1 and cosig2
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << (m_consensusObject->GetConsensusErrorMsg()));

            // Block till txn is fetched
            unique_lock<mutex> lock(m_mutexCVMicroBlockMissingTxn);
            if (cv_MicroBlockMissingTxn.wait_for(
                    lock, chrono::seconds(FETCHING_MISSING_TXNS_TIMEOUT))
                == std::cv_status::timeout)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "fetching missing txn timeout");
            }
            else
            {
                // Re-run consensus
                m_consensusObject->RecoveryAndProcessFromANewState(
                    ConsensusCommon::INITIAL);

                auto rerunconsensus = [this, message, offset, from]() {
                    ProcessMicroblockConsensus(message, offset, from);
                };
                DetachedFunction(1, rerunconsensus);
                return true;
            }
        }
        else
        {
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - unhandled consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << m_consensusObject->GetConsensusErrorMsg());
        }

        // return false;
        // TODO: Optimize state transition.
        LOG_GENERAL(WARNING,
                    "ConsensusCommon::State::ERROR here, but we move on.");

        SetState(WAITING_FINALBLOCK); // Move on to next Epoch.

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "If I received a new Finalblock from DS committee. I will "
                  "still process it");
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << m_consensusObject->GetStateString());

        cv_processConsensusMessage.notify_all();
    }
#endif // IS_LOOKUP_NODE
    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::ComposeMicroBlock()
{
    // To-do: Replace dummy values with the required ones
    LOG_MARKER();

    // TxBlockHeader
    uint8_t type = TXBLOCKTYPE::MICRO;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint32_t shardID = m_myShardID;
    uint256_t gasLimit = 100;
    uint256_t gasUsed = 1;
    BlockHash prevHash;
    fill(prevHash.asArray().begin(), prevHash.asArray().end(), 0x77);
    uint64_t blockNum = m_mediator.m_currentEpochNum;
    uint256_t timestamp = get_time_as_int();
    TxnHash txRootHash;
    uint32_t numTxs = 0;
    const PubKey& minerPubKey = m_mediator.m_selfKey.second;
    uint64_t dsBlockNum = m_mediator.m_currentEpochNum;
    BlockHash dsBlockHeader;
    fill(dsBlockHeader.asArray().begin(), dsBlockHeader.asArray().end(), 0x11);
    StateHash stateDeltaHash = AccountStore::GetInstance().GetStateDeltaHash();

    // TxBlock
    vector<TxnHash> tranHashes;

    unsigned int index = 0;
    {
        lock(m_mutexReceivedTransactions, m_mutexSubmittedTransactions);
        lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
        lock_guard<mutex> g2(m_mutexSubmittedTransactions, adopt_lock);

        auto& receivedTransactions = m_receivedTransactions[blockNum];
        auto& submittedTransactions = m_submittedTransactions[blockNum];

        txRootHash = ComputeTransactionsRoot(receivedTransactions,
                                             submittedTransactions);

        numTxs = receivedTransactions.size() + submittedTransactions.size();
        tranHashes.resize(numTxs);
        for (const auto& tx : receivedTransactions)
        {
            const auto& txid = tx.first.asArray();
            copy(txid.begin(), txid.end(),
                 tranHashes.at(index).asArray().begin());
            index++;
        }

        for (const auto& tx : submittedTransactions)
        {
            const auto& txid = tx.first.asArray();
            copy(txid.begin(), txid.end(),
                 tranHashes.at(index).asArray().begin());
            index++;
        }
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Creating new micro block.")
    m_microblock.reset(new MicroBlock(
        MicroBlockHeader(type, version, shardID, gasLimit, gasUsed, prevHash,
                         blockNum, timestamp, txRootHash, numTxs, minerPubKey,
                         dsBlockNum, dsBlockHeader, stateDeltaHash),
        tranHashes, CoSignatures()));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Micro block proposed with "
                  << m_microblock->GetHeader().GetNumTxs()
                  << " transactions for epoch "
                  << m_mediator.m_currentEpochNum);

    return true;
}

bool Node::OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
                             unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    if (errorMsg.size() < 2 * sizeof(uint32_t) + offset)
    {
        LOG_GENERAL(WARNING, "Malformed Message");
        return false;
    }

    uint32_t numOfAbsentHashes
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint64_t blockNum
        = Serializable::GetNumber<uint64_t>(errorMsg, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    vector<TxnHash> missingTransactions;

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        TxnHash txnHash;
        copy(errorMsg.begin() + offset,
             errorMsg.begin() + offset + TRAN_HASH_SIZE,
             txnHash.asArray().begin());
        offset += TRAN_HASH_SIZE;

        missingTransactions.emplace_back(txnHash);
    }

    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    lock(m_mutexReceivedTransactions, m_mutexSubmittedTransactions);
    lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexSubmittedTransactions, adopt_lock);

    auto& receivedTransactions = m_receivedTransactions[blockNum];
    auto& submittedTransactions = m_submittedTransactions[blockNum];

    unsigned int cur_offset = 0;
    vector<unsigned char> tx_message
        = {MessageType::NODE, NodeInstructionType::SUBMITTRANSACTION};
    cur_offset += MessageOffset::BODY;
    tx_message.push_back(SUBMITTRANSACTIONTYPE::MISSINGTXN);
    cur_offset += MessageOffset::INST;
    Serializable::SetNumber<uint64_t>(tx_message, cur_offset, blockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        // LOG_GENERAL(INFO, "Peer " << from << " : " << portNo << " missing txn " << missingTransactions[i])

        Transaction t;
        if (submittedTransactions.find(missingTransactions[i])
            != submittedTransactions.end())
        {
            t = submittedTransactions[missingTransactions[i]];
        }
        else if (receivedTransactions.find(missingTransactions[i])
                 != receivedTransactions.end())
        {
            t = receivedTransactions[missingTransactions[i]];
        }
        else
        {
            LOG_GENERAL(INFO,
                        "Leader unable to find txn proposed in microblock "
                            << missingTransactions[i]);
            // throw exception();
            // return false;
            continue;
        }
        t.Serialize(tx_message, cur_offset);
        cur_offset += t.GetSerializedSize();
    }
    P2PComm::GetInstance().SendMessage(peer, tx_message);

    return true;
}

bool Node::OnCommitFailure([
    [gnu::unused]] const std::map<unsigned int, std::vector<unsigned char>>&
                               commitFailureMap)
{
    LOG_MARKER();

    // for(auto failureEntry: commitFailureMap)
    // {

    // }

    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //           "Going to sleep before restarting consensus");

    // std::this_thread::sleep_for(30s);
    // RunConsensusOnMicroBlockWhenShardLeader();

    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //           "Woke from sleep after consensus restart");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Microblock consensus failed, going to wait for final block "
              "announcement");

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. Creating microblock for epoch"
                  << m_mediator.m_currentEpochNum);

    // composed microblock stored in m_microblock
    ComposeMicroBlock();

    vector<unsigned char> microblock;
    m_microblock->Serialize(microblock, 0);

    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. "
                  << " m_consensusID: " << m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << m_myShardMembersNetworkInfo[m_consensusLeaderID]);

    auto nodeMissingTxnsFunc
        = [this](const vector<unsigned char>& errorMsg, unsigned int offset,
                 const Peer& from) mutable -> bool {
        return OnNodeMissingTxns(errorMsg, offset, from);
    };

    auto commitFailureFunc
        = [this](const map<unsigned int, vector<unsigned char>>& m) mutable
        -> bool { return OnCommitFailure(m); };

    deque<pair<PubKey, Peer>> peerList;
    auto it1 = m_myShardMembersPubKeys.begin();
    auto it2 = m_myShardMembersNetworkInfo.begin();

    while (it1 != m_myShardMembersPubKeys.end())
    {
        peerList.push_back(make_pair(*it1, *it2));
        ++it1;
        ++it2;
    }

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, peerList, static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), nodeMissingTxnsFunc,
        commitFailureFunc));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    LOG_STATE("[MICON][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] BGIN");

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());
    cl->StartConsensus(microblock, MicroBlockHeader::SIZE);

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardBackup()
{
    LOG_MARKER();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am a backup node. Waiting for microblock announcement for epoch "
            << m_mediator.m_currentEpochNum);
    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return MicroBlockValidator(message, errorMsg);
    };

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard backup. "
                  << " m_consensusID: " << m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << m_myShardMembersNetworkInfo[m_consensusLeaderID]);

    deque<pair<PubKey, Peer>> peerList;
    auto it1 = m_myShardMembersPubKeys.begin();
    auto it2 = m_myShardMembersNetworkInfo.begin();

    while (it1 != m_myShardMembersPubKeys.end())
    {
        peerList.push_back(make_pair(*it1, *it2));
        ++it1;
        ++it2;
    }

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first, peerList,
        static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

bool Node::RunConsensusOnMicroBlock()
{
    LOG_MARKER();

    // set state first and then take writer lock so that SubmitTransactions
    // if it takes reader lock later breaks out of loop

    InitCoinbase();

    SetState(MICROBLOCK_CONSENSUS_PREP);

    AccountStore::GetInstance().SerializeDelta();

    {
        lock_guard<mutex> g2(m_mutexNewRoundStarted);
        m_newRoundStarted = false;
    }

    if (m_isPrimary == true)
    {
        if (!RunConsensusOnMicroBlockWhenShardLeader())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardLeader");
            // throw exception();
            return false;
        }
    }
    else
    {
        if (!RunConsensusOnMicroBlockWhenShardBackup())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardBackup");
            // throw exception();
            return false;
        }
    }

    SetState(MICROBLOCK_CONSENSUS);
    cv_microblockConsensusObject.notify_all();
    return true;
}

bool Node::CheckBlockTypeIsMicro()
{
    // Check type (must be micro block type)
    if (m_microblock->GetHeader().GetType() != TXBLOCKTYPE::MICRO)
    {
        LOG_GENERAL(WARNING,
                    "Type check failed. Expected: "
                        << (unsigned int)TXBLOCKTYPE::MICRO << " Actual: "
                        << (unsigned int)m_microblock->GetHeader().GetType());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK);

        return false;
    }

    LOG_GENERAL(INFO, "Type check passed");

    return true;
}

bool Node::CheckMicroBlockVersion()
{
    // Check version (must be most current version)
    if (m_microblock->GetHeader().GetVersion() != BLOCKVERSION::VERSION1)
    {
        LOG_GENERAL(
            WARNING,
            "Version check failed. Expected: "
                << (unsigned int)BLOCKVERSION::VERSION1 << " Actual: "
                << (unsigned int)m_microblock->GetHeader().GetVersion());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_VERSION);

        return false;
    }

    LOG_GENERAL(INFO, "Version check passed");

    return true;
}

bool Node::CheckMicroBlockTimestamp()
{
    // Check timestamp (must be greater than timestamp of last Tx block header in the Tx blockchain)
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        const TxBlock& lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
        uint256_t thisMicroblockTimestamp
            = m_microblock->GetHeader().GetTimestamp();
        uint256_t lastTxBlockTimestamp = lastTxBlock.GetHeader().GetTimestamp();
        if (thisMicroblockTimestamp <= lastTxBlockTimestamp)
        {
            LOG_GENERAL(WARNING,
                        "Timestamp check failed. Last Tx Block: "
                            << lastTxBlockTimestamp
                            << " Microblock: " << thisMicroblockTimestamp);

            m_consensusObject->SetConsensusErrorCode(
                ConsensusCommon::INVALID_TIMESTAMP);

            return false;
        }
    }

    LOG_GENERAL(INFO, "Timestamp check passed");

    return true;
}

bool Node::CheckLegitimacyOfTxnHashes(vector<unsigned char>& errorMsg)
{
    lock(m_mutexReceivedTransactions, m_mutexSubmittedTransactions);
    lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexSubmittedTransactions, adopt_lock);

    auto const& receivedTransactions
        = m_receivedTransactions[m_mediator.m_currentEpochNum];
    auto const& submittedTransactions
        = m_submittedTransactions[m_mediator.m_currentEpochNum];

    m_numOfAbsentTxnHashes = 0;

    int offset = 0;

    for (auto const& hash : m_microblock->GetTranHashes())
    {
        // Check if transaction is part of submitted Tx list
        if (submittedTransactions.find(hash) != submittedTransactions.end())
        {
            continue;
        }

        // Check if transaction is part of received Tx list
        if (receivedTransactions.find(hash) == receivedTransactions.end())
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Missing txn: " << hash)
            if (errorMsg.size() == 0)
            {
                errorMsg.resize(sizeof(uint32_t) + sizeof(uint64_t)
                                + TRAN_HASH_SIZE);
                offset += (sizeof(uint32_t) + sizeof(uint64_t));
            }
            else
            {
                errorMsg.resize(offset + TRAN_HASH_SIZE);
            }
            copy(hash.asArray().begin(), hash.asArray().end(),
                 errorMsg.begin() + offset);
            offset += TRAN_HASH_SIZE;
            m_numOfAbsentTxnHashes++;
        }
    }

    if (m_numOfAbsentTxnHashes)
    {
        Serializable::SetNumber<uint32_t>(errorMsg, 0, m_numOfAbsentTxnHashes,
                                          sizeof(uint32_t));
        Serializable::SetNumber<uint64_t>(errorMsg, sizeof(uint32_t),
                                          m_mediator.m_currentEpochNum,
                                          sizeof(uint64_t));
        return false;
    }

    return true;
}

bool Node::CheckMicroBlockHashes(vector<unsigned char>& errorMsg)
{
    // Check transaction hashes (number of hashes must be = Tx count field)
    uint32_t txhashessize = m_microblock->GetTranHashes().size();
    uint32_t numtxs = m_microblock->GetHeader().GetNumTxs();
    if (txhashessize != numtxs)
    {
        LOG_GENERAL(WARNING,
                    "Tx hashes check failed. Tx hashes size: "
                        << txhashessize << " Num txs: " << numtxs);

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_BLOCK_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Hash count check passed");

    // Check if I have the txn bodies corresponding to the hashes included in the microblock
    if (!CheckLegitimacyOfTxnHashes(errorMsg))
    {
        LOG_GENERAL(WARNING,
                    "Missing a txn hash included in proposed microblock");

        m_consensusObject->SetConsensusErrorCode(ConsensusCommon::MISSING_TXN);

        return false;
    }

    LOG_GENERAL(INFO, "Hash legitimacy check passed");

    return true;
}

bool Node::CheckMicroBlockTxnRootHash()
{
    // Check transaction root
    TxnHash expectedTxRootHash
        = ComputeTransactionsRoot(m_microblock->GetTranHashes());

    LOG_GENERAL(
        INFO,
        "Microblock root computation done "
            << DataConversion::charArrToHexStr(expectedTxRootHash.asArray()));
    LOG_GENERAL(INFO,
                "Expected root: " << DataConversion::charArrToHexStr(
                    m_microblock->GetHeader().GetTxRootHash().asArray()));

    if (expectedTxRootHash != m_microblock->GetHeader().GetTxRootHash())
    {
        LOG_GENERAL(WARNING, "Txn root does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_ROOT_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Root check passed");

    return true;
}

bool Node::CheckMicroBlockStateDeltaHash()
{
    StateHash expectedStateDeltaHash
        = AccountStore::GetInstance().GetStateDeltaHash();

    LOG_GENERAL(INFO,
                "Microblock state delta generation done "
                    << DataConversion::charArrToHexStr(
                           expectedStateDeltaHash.asArray()));
    LOG_GENERAL(INFO,
                "Expected root: " << DataConversion::charArrToHexStr(
                    m_microblock->GetHeader().GetStateDeltaHash().asArray()));

    if (expectedStateDeltaHash != m_microblock->GetHeader().GetStateDeltaHash())
    {
        LOG_GENERAL(WARNING, "State delta hash does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_STATE_DELTA_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "State delta hash check passed");

    return true;
}

bool Node::CheckMicroBlockShardID()
{
    // Check version (must be most current version)
    if (m_microblock->GetHeader().GetShardID() != m_myShardID)
    {
        LOG_GENERAL(WARNING,
                    "ShardID check failed. Expected: "
                        << m_myShardID << " Actual: "
                        << m_microblock->GetHeader().GetShardID());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_SHARD_ID);

        return false;
    }

    LOG_GENERAL(INFO, "ShardID check passed");

    return true;
}

bool Node::MicroBlockValidator(const vector<unsigned char>& microblock,
                               vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // [TODO] To put in the logic
    m_microblock = make_shared<MicroBlock>(MicroBlock(microblock, 0));

    bool valid = false;

    do
    {
        if (!CheckBlockTypeIsMicro() || !CheckMicroBlockVersion()
            || !CheckMicroBlockTimestamp() || !CheckMicroBlockHashes(errorMsg)
            || !CheckMicroBlockTxnRootHash() || !CheckMicroBlockStateDeltaHash()
            || !CheckMicroBlockShardID())
        {
            break;
        }

        // Check gas limit (must satisfy some equations)
        // Check gas used (must be <= gas limit)
        // Check state root (TBD)
        // Check pubkey (must be valid and = shard leader)
        // Check parent DS hash (must be = digest of last DS block header in the DS blockchain)
        // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)
        // Check parent DS block number (must be = block number of last DS block header in the DS blockchain)
        // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)

        valid = true;
    } while (false);

    if (!valid)
    {
        m_microblock = nullptr;
        Serializable::SetNumber<uint32_t>(
            errorMsg, errorMsg.size(), m_mediator.m_selfPeer.m_listenPortHost,
            sizeof(uint32_t));
        // LOG_GENERAL(INFO, "To-do: What to do if proposed microblock is not valid?");
        return false;
    }

    return valid;

    // #else // IS_LOOKUP_NODE

    // return true;
}
#endif // IS_LOOKUP_NODE
