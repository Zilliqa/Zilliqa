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

#include <algorithm>
#include <chrono>
#include <thread>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void DirectoryService::StoreDSBlockToStorage()
{
    LOG_MARKER();
    lock_guard<mutex> g(m_mutexPendingDSBlock);
    int result = m_mediator.m_dsBlockChain.AddBlock(*m_pendingDSBlock);
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "Storing DS Block Number: "
            << m_pendingDSBlock->GetHeader().GetBlockNum()
            << " with Nonce: " << m_pendingDSBlock->GetHeader().GetNonce()
            << ", Difficulty: " << m_pendingDSBlock->GetHeader().GetDifficulty()
            << ", Timestamp: " << m_pendingDSBlock->GetHeader().GetTimestamp()
            << ", vc count: "
            << m_pendingDSBlock->GetHeader().GetViewChangeCount());
    if (result == -1)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "We failed to add pendingdsblock to dsblockchain.");
        // throw exception();
    }

    // Store DS Block to disk
    vector<unsigned char> serializedDSBlock;
    m_pendingDSBlock->Serialize(serializedDSBlock, 0);
    BlockStorage::GetBlockStorage().PutDSBlock(
        m_pendingDSBlock->GetHeader().GetBlockNum(), serializedDSBlock);
    BlockStorage::GetBlockStorage().PushBackTxBodyDB(
        m_pendingDSBlock->GetHeader().GetBlockNum());
}

bool DirectoryService::SendDSBlockToLookupNodes(DSBlock& lastDSBlock,
                                                Peer& winnerpeer)
{
    vector<unsigned char> dsblock_message
        = {MessageType::NODE, NodeInstructionType::DSBLOCK};
    unsigned int curr_offset = MessageOffset::BODY;

    // 259-byte DS block
    lastDSBlock.Serialize(dsblock_message, MessageOffset::BODY);
    curr_offset += lastDSBlock.GetSerializedSize();

    // 32-byte DS block hash / rand1
    dsblock_message.resize(dsblock_message.size() + BLOCK_HASH_SIZE);
    copy(m_mediator.m_dsBlockRand.begin(), m_mediator.m_dsBlockRand.end(),
         dsblock_message.begin() + curr_offset);
    curr_offset += BLOCK_HASH_SIZE;

    // 16-byte winner IP and 4-byte winner port
    winnerpeer.Serialize(dsblock_message, curr_offset);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the primary DS am sending the DSBlock to the lookup nodes");
    m_mediator.m_lookup->SendMessageToLookupNodes(dsblock_message);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the primary DS have sent the DSBlock to the lookup nodes");

    return true;
}

void DirectoryService::DetermineNodesToSendDSBlockTo(
    const Peer& winnerpeer, unsigned int& my_DS_cluster_num,
    unsigned int& my_pow1nodes_cluster_lo,
    unsigned int& my_pow1nodes_cluster_hi) const
{
    // Multicast DSBLOCK message to everyone in the network
    // Multicast assignments:
    // 1. Divide DS committee into clusters of size 20
    // 2. Divide the sorted PoW1 submitters into (DS committee / 20) clusters
    // Message = [259-byte DS block] [32-byte DS block hash / rand1] [16-byte winner IP] [4-byte winner port]
    LOG_MARKER();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "New DSBlock created with chosen nonce   = 0x"
            << hex
            << m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetNonce());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New DSBlock hash is                     = 0x"
                  << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New DS leader (PoW1 winner) IP          = "
                  << winnerpeer.GetPrintableIPAddress() << ":"
                  << winnerpeer.m_listenPortHost);

    unsigned int num_DS_clusters = m_mediator.m_DSCommitteeNetworkInfo.size()
        / DS_MULTICAST_CLUSTER_SIZE;
    if ((m_mediator.m_DSCommitteeNetworkInfo.size() % DS_MULTICAST_CLUSTER_SIZE)
        > 0)
    {
        num_DS_clusters++;
    }

    unsigned int pow1nodes_cluster_size
        = m_allPoWConns.size() / num_DS_clusters;
    if ((m_allPoWConns.size() % num_DS_clusters) > 0)
    {
        pow1nodes_cluster_size++;
    }

    my_DS_cluster_num = m_consensusMyID / DS_MULTICAST_CLUSTER_SIZE;
    my_pow1nodes_cluster_lo = my_DS_cluster_num * pow1nodes_cluster_size;
    my_pow1nodes_cluster_hi
        = my_pow1nodes_cluster_lo + pow1nodes_cluster_size - 1;

    if (my_pow1nodes_cluster_hi >= m_allPoWConns.size())
    {
        my_pow1nodes_cluster_hi = m_allPoWConns.size() - 1;
    }
}

void DirectoryService::SendDSBlockToCluster(
    const Peer& winnerpeer, unsigned int my_pow1nodes_cluster_lo,
    unsigned int my_pow1nodes_cluster_hi)
{
    // Message = [259-byte DS block] [32-byte DS block hash / rand1] [16-byte winner IP] [4-byte winner port]
    vector<unsigned char> dsblock_message
        = {MessageType::NODE, NodeInstructionType::DSBLOCK};
    unsigned int curr_offset = MessageOffset::BODY;

    // 259-byte DS block
    m_mediator.m_dsBlockChain.GetLastBlock().Serialize(dsblock_message,
                                                       MessageOffset::BODY);
    curr_offset += m_mediator.m_dsBlockChain.GetLastBlock().GetSerializedSize();

    // 32-byte DS block hash / rand1
    dsblock_message.resize(dsblock_message.size() + BLOCK_HASH_SIZE);
    copy(m_mediator.m_dsBlockRand.begin(), m_mediator.m_dsBlockRand.end(),
         dsblock_message.begin() + curr_offset);
    curr_offset += BLOCK_HASH_SIZE;

    // 16-byte winner IP and 4-byte winner port
    winnerpeer.Serialize(dsblock_message, curr_offset);

    vector<Peer> pow1nodes_cluster;

    auto p = m_allPoWConns.begin();
    advance(p, my_pow1nodes_cluster_lo);

    for (unsigned int i = my_pow1nodes_cluster_lo; i <= my_pow1nodes_cluster_hi;
         i++)
    {
        pow1nodes_cluster.push_back(p->second);
        p++;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Multicasting DSBLOCK message to PoW1 nodes "
                  << my_pow1nodes_cluster_lo << " to "
                  << my_pow1nodes_cluster_hi);

#ifdef STAT_TEST
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(dsblock_message);
    vector<unsigned char> this_msg_hash = sha256.Finalize();
    LOG_STATE(
        "[INFOR]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6) << "]["
        << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
               .substr(0, 6)
        << "]["
        << m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        << "] DSBLOCKGEN");
#endif // STAT_TEST

    // Sleep to give sufficient time to other ds node to receive the ds block
    this_thread::sleep_for(chrono::seconds(5));
    P2PComm::GetInstance().SendBroadcastMessage(pow1nodes_cluster,
                                                dsblock_message);
}

void DirectoryService::UpdateMyDSModeAndConsensusId()
{
    // If I was DS primary, now I will only be DS backup
    if (m_mode == PRIMARY_DS)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am now just a backup DS");
        m_mode = BACKUP_DS;
        m_consensusMyID++;

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << setw(6) << left << m_consensusMyID
                             << "] DSBK");
#endif // STAT_TEST
    }
    // Check if I am the oldest backup DS (I will no longer be part of the DS committee)
    else if ((uint32_t)(m_consensusMyID + 1)
             == m_mediator.m_DSCommitteeNetworkInfo.size())
    {
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I am the oldest backup DS -> now kicked out of DS committee :-(");
        m_mediator.m_node->SetState(Node::NodeState::POW2_SUBMISSION);
        m_mode = IDLE;

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][      ] IDLE");
#endif // STAT_TEST
    }
    // Other DS nodes continue to remain DS backups
    else
    {
        m_consensusMyID++;

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << setw(6) << left << m_consensusMyID
                             << "] DSBK");
#endif // STAT_TEST
    }
}

void DirectoryService::UpdateDSCommiteeComposition(const Peer& winnerpeer)
{
    // Update the DS committee composition
    LOG_MARKER();

    m_mediator.m_DSCommitteeNetworkInfo.push_front(winnerpeer);
    m_mediator.m_DSCommitteePubKeys.push_front(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetMinerPubKey());
    m_mediator.m_DSCommitteeNetworkInfo.pop_back();
    m_mediator.m_DSCommitteePubKeys.pop_back();

    // Remove the new winner of pow1 from m_allpowconn. He is the new ds leader and do not need to do pow anymore
    m_allPoWConns.erase(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetMinerPubKey());
}

void DirectoryService::ScheduleShardingConsensus(const unsigned int wait_window)
{
    LOG_MARKER();

    auto func = [this, wait_window]() -> void {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Waiting " << wait_window
                             << " seconds, accepting PoW2 submissions...");
        this_thread::sleep_for(chrono::seconds(wait_window));
        RunConsensusOnSharding();
    };
    DetachedFunction(1, func);
}

void DirectoryService::ProcessDSBlockConsensusWhenDone(
    const vector<unsigned char>& message, unsigned int offset)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DS block consensus is DONE!!!");

#ifdef STAT_TEST
    if (m_mode == PRIMARY_DS)
    {
        LOG_STATE("[DSCON]["
                  << setw(15) << left
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                  << m_mediator.m_txBlockChain.GetBlockCount() << "] DONE");
    }
#endif // STAT_TEST

    {
        lock_guard<mutex> g(m_mutexPendingDSBlock);
        assert(m_pendingDSBlock != nullptr);

        // Update the DS block with the co-signatures from the consensus
        m_pendingDSBlock->SetCoSignatures(*m_consensusObject);

        if (m_pendingDSBlock->GetHeader().GetBlockNum()
            == m_mediator.m_dsBlockChain.GetBlockCount() + 1)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "We are missing some blocks. What to do here?");
        }
    }

    // Add the DS block to the chain
    StoreDSBlockToStorage();
    DSBlock lastDSBlock = m_mediator.m_dsBlockChain.GetLastBlock();

    m_mediator.UpdateDSBlockRand();

    Peer winnerpeer;
    {
        lock_guard<mutex> g(m_mutexAllPoWConns);
        winnerpeer = m_allPoWConns.at(lastDSBlock.GetHeader().GetMinerPubKey());
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DSBlock to be sent to the lookup nodes");

    // TODO: Refine this
    unsigned int nodeToSendToLookUpLo = COMM_SIZE / 4;
    unsigned int nodeToSendToLookUpHi
        = nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

    if (m_consensusMyID > nodeToSendToLookUpLo
        && m_consensusMyID < nodeToSendToLookUpHi)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I the DS folks that will soon be sending the DSBlock to the "
                  "lookup nodes");
        SendDSBlockToLookupNodes(lastDSBlock, winnerpeer);
    }

    unsigned int my_DS_cluster_num, my_pow1nodes_cluster_lo,
        my_pow1nodes_cluster_hi;
    DetermineNodesToSendDSBlockTo(winnerpeer, my_DS_cluster_num,
                                  my_pow1nodes_cluster_lo,
                                  my_pow1nodes_cluster_hi);

    // Too few target nodes - avoid asking all DS clusters to send
    if ((my_DS_cluster_num + 1) <= m_allPoWConns.size())
    {
        SendDSBlockToCluster(winnerpeer, my_pow1nodes_cluster_lo,
                             my_pow1nodes_cluster_hi);
    }

    {
        lock_guard<mutex> g(m_mutexAllPOW1);
        m_allPoW1s.clear();
    }

    UpdateDSCommiteeComposition(winnerpeer);
    UpdateMyDSModeAndConsensusId();

    if (m_mode != IDLE)
    {
        SetState(POW2_SUBMISSION);
        ScheduleShardingConsensus(BACKUP_POW2_WINDOW_IN_SECONDS);
    }
    else
    {
        // Tell my Node class to start PoW2
        m_mediator.UpdateDSBlockRand();
        array<unsigned char, 32> rand2{};
        this_thread::sleep_for(chrono::seconds(3));
        m_mediator.m_node->StartPoW2(lastDSBlock.GetHeader().GetBlockNum(),
                                     POW2_DIFFICULTY, m_mediator.m_dsBlockRand,
                                     rand2);
    }
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessDSBlockConsensus(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();
    // Consensus messages must be processed in correct sequence as they come in
    // It is possible for ANNOUNCE to arrive before correct DS state
    // In that case, ANNOUNCE will sleep for a second below
    // If COLLECTIVESIG also comes in, it's then possible COLLECTIVESIG will be processed before ANNOUNCE!
    // So, ANNOUNCE should acquire a lock here

    lock_guard<mutex> g(m_mutexConsensus);

    // Wait for a while for ProcessDSBlock in the case that primary sent announcement pretty early
    // TODO: Should await sleep. Use conditional var.
    unsigned int sleep_time_while_waiting = 100;
    if ((m_state == POW1_SUBMISSION) || (m_state == DSBLOCK_CONSENSUS_PREP))
    {
        //for (unsigned int i = 0; i < POW1_WINDOW_IN_SECONDS; i++)
        unsigned int i = 0;
        while (true)
        {
            if (m_state == DSBLOCK_CONSENSUS)
            {
                break;
            }

            if (i % 100 == 0)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for DSBLOCK_CONSENSUS before processing. "
                          "Time elapsed: "
                              << (i / 10) << "seconds");
            }

            this_thread::sleep_for(
                chrono::milliseconds(sleep_time_while_waiting));
            i++;
        }
    }

    if (!CheckState(PROCESS_DSBLOCKCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Ignoring consensus message");
        return false;
    }

    bool result = m_consensusObject->ProcessMessage(message, offset, from);
    ConsensusCommon::State state = m_consensusObject->GetState();

    if (state == ConsensusCommon::State::DONE)
    {
        m_viewChangeCounter = 0;
        cv_RecoveryDSBlockConsensus.notify_all();
        ProcessDSBlockConsensusWhenDone(message, offset);
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DEBUG for verify sig m_allPoWConns  size is "
                << m_allPoWConns.size()
                << ". Please check numbers of pow1 receivied by this node");

        // Wait for view change to happen
        //throw exception();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << state);
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}
