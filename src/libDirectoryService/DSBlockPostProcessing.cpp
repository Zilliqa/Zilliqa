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
#include "libNetwork/Whitelist.h"
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
            << m_pendingDSBlock->GetHeader().GetBlockNum() << " with Nonce: "
            << m_pendingDSBlock->GetHeader().GetNonce() << ", Difficulty: "
            << to_string(m_pendingDSBlock->GetHeader().GetDifficulty())
            << ", Timestamp: " << m_pendingDSBlock->GetHeader().GetTimestamp());

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
    m_latestActiveDSBlockNum = m_pendingDSBlock->GetHeader().GetBlockNum();
    BlockStorage::GetBlockStorage().PutMetadata(
        LATESTACTIVEDSBLOCKNUM,
        DataConversion::StringToCharArray(to_string(m_latestActiveDSBlockNum)));
}

void DirectoryService::SendDSBlockToLookupNodes()
{
    // Message = [DS block] [PoW winner IP] [Sharding structure] [Txn sharing assignments]
    // This is the same as the DS Block consensus announcement message

    vector<unsigned char> dsblock_message(MessageOffset::BODY
                                          + m_PoWConsensusMessage.size());
    dsblock_message.at(MessageOffset::TYPE) = MessageType::NODE;
    dsblock_message.at(MessageOffset::INST) = NodeInstructionType::DSBLOCK;
    copy(m_PoWConsensusMessage.begin(), m_PoWConsensusMessage.end(),
         dsblock_message.begin() + MessageOffset::BODY);

    m_mediator.m_lookup->SendMessageToLookupNodes(dsblock_message);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the part of the subset of the DS committee that have sent the "
              "DSBlock to the lookup nodes");
}

void DirectoryService::SendDSBlockToNewDSLeader(const Peer& winnerpeer)
{
    // Message = [Shard ID] [DS block] [PoW winner IP] [Sharding structure] [Txn sharing assignments]
    // This is the same as the DS Block consensus announcement message, plus the additional Shard ID
    // [Shard ID] is dummy here since the new DS leader won't be part of the shards

    vector<unsigned char> dsblock_message(MessageOffset::BODY + sizeof(uint32_t)
                                          + m_PoWConsensusMessage.size());
    dsblock_message.at(MessageOffset::TYPE) = MessageType::NODE;
    dsblock_message.at(MessageOffset::INST) = NodeInstructionType::DSBLOCK;

    // [Shard ID]
    Serializable::SetNumber<uint32_t>(dsblock_message, MessageOffset::BODY, 0,
                                      sizeof(uint32_t));

    // [DS block] [PoW winner IP] [Sharding structure] [Txn sharing assignments]
    copy(m_PoWConsensusMessage.begin(), m_PoWConsensusMessage.end(),
         dsblock_message.begin() + MessageOffset::BODY + sizeof(uint32_t));

    P2PComm::GetInstance().SendMessage(winnerpeer, dsblock_message);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the part of the subset of the DS committee that have sent the "
              "DSBlock to the new DS leader");
}

void DirectoryService::SetupMulticastConfigForDSBlock(
    unsigned int& my_DS_cluster_num, unsigned int& my_shards_lo,
    unsigned int& my_shards_hi) const
{
    LOG_MARKER();

    unsigned int num_DS_clusters
        = m_mediator.m_DSCommittee->size() / DS_MULTICAST_CLUSTER_SIZE;
    if ((m_mediator.m_DSCommittee->size() % DS_MULTICAST_CLUSTER_SIZE) > 0)
    {
        // If there are still ds lefts, add a new ds cluster
        num_DS_clusters++;
    }

    unsigned int shard_groups_count = m_shards.size() / num_DS_clusters;
    if ((m_shards.size() % num_DS_clusters) > 0)
    {
        // If there is still nodes, increase num of shard
        shard_groups_count++;
    }

    my_DS_cluster_num = m_consensusMyID / DS_MULTICAST_CLUSTER_SIZE;
    my_shards_lo = my_DS_cluster_num * shard_groups_count;
    my_shards_hi = my_shards_lo + shard_groups_count
        - 1; // Multicast configuration to my assigned shard's nodes - send SHARDING message
    if (my_shards_hi >= m_shards.size())
    {
        my_shards_hi = m_shards.size() - 1;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "my_shards_lo: "
                  << my_shards_lo << " my_shards_hi: " << my_shards_hi
                  << " my_DS_cluster_num  : " << my_DS_cluster_num);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "shard_groups_count : " << shard_groups_count
                                      << " m shard size       : "
                                      << m_shards.size());
}

void DirectoryService::SendDSBlockToShardNodes(unsigned int my_shards_lo,
                                               unsigned int my_shards_hi)
{
    // Message = [Shard ID] [DS block] [PoW winner IP] [Sharding structure] [Txn sharing assignments]
    // This is the same as the DS Block consensus announcement message, plus the additional Shard ID

    vector<unsigned char> dsblock_message(MessageOffset::BODY + sizeof(uint32_t)
                                          + m_PoWConsensusMessage.size());
    dsblock_message.at(MessageOffset::TYPE) = MessageType::NODE;
    dsblock_message.at(MessageOffset::INST) = NodeInstructionType::DSBLOCK;
    copy(m_PoWConsensusMessage.begin(), m_PoWConsensusMessage.end(),
         dsblock_message.begin() + MessageOffset::BODY + sizeof(uint32_t));

    auto p = m_shards.begin();
    advance(p, my_shards_lo);
    for (unsigned int i = my_shards_lo; i <= my_shards_hi; i++)
    {
        // [Shard ID] -> get from the leader's info in m_publicKeyToShardIdMap
        Serializable::SetNumber<uint32_t>(
            dsblock_message, MessageOffset::BODY,
            m_publicKeyToShardIdMap.at(p->begin()->first), sizeof(uint32_t));

        // Send the message
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
        sha256.Update(dsblock_message);
        vector<unsigned char> this_msg_hash = sha256.Finalize();

        LOG_STATE(
            "[INFOR]["
            << std::setw(15) << std::left
            << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
            << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
            << "]["
            << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
                   .substr(0, 6)
            << "]["
            << m_mediator.m_txBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum()
                + 1
            << "] SHMSG");

        vector<Peer> shard_peers;
        for (auto& kv : *p)
        {
            shard_peers.emplace_back(kv.second);
        }

        P2PComm::GetInstance().SendBroadcastMessage(shard_peers,
                                                    dsblock_message);
        p++;
    }
}

void DirectoryService::UpdateMyDSModeAndConsensusId()
{
    // If I was DS primary, now I will only be DS backup
    if (m_mode == PRIMARY_DS)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am now just a backup DS");
        LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                      DS_BACKUP_MSG);
        m_mode = BACKUP_DS;
        m_consensusMyID++;

        LOG_STATE("[IDENT][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << setw(6) << left << m_consensusMyID
                             << "] DSBK");
    }
    // Check if I am the oldest backup DS (I will no longer be part of the DS committee)
    else if ((uint32_t)(m_consensusMyID + 1)
             == m_mediator.m_DSCommittee->size())
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am the oldest backup DS -> I am now just a shard node"
                      << "\n"
                      << DS_KICKOUT_MSG);
        m_mode = IDLE;

        LOG_STATE("[IDENT][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][      ] IDLE");
    }
    // Other DS nodes continue to remain DS backups
    else
    {
        m_consensusMyID++;

        LOG_STATE("[IDENT][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << setw(6) << left << m_consensusMyID
                             << "] DSBK");
    }
}

void DirectoryService::UpdateDSCommiteeComposition(const Peer& winnerpeer)
{
    // Update the DS committee composition
    LOG_MARKER();

    m_mediator.m_DSCommittee->emplace_front(make_pair(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetMinerPubKey(),
        winnerpeer));
    m_mediator.m_DSCommittee->pop_back();

    // Remove the new winner of pow from m_allpowconn. He is the new ds leader and do not need to do pow anymore
    m_allPoWConns.erase(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetMinerPubKey());
}

void DirectoryService::StartFirstTxEpoch()
{
    LOG_MARKER();

    {
        lock_guard<mutex> g(m_mutexAllPOW);
        m_allPoWs.clear();
    }

    m_viewChangeCounter = 0;

    if (m_mode != IDLE)
    {
        if (TEST_NET_MODE)
        {
            LOG_GENERAL(INFO, "Updating shard whitelist");
            Whitelist::GetInstance().UpdateShardWhitelist();
        }

        // Start sharding work
        SetState(MICROBLOCK_SUBMISSION);

        // Check for state change. If it get stuck at microblock submission for too long, move on to finalblock without the microblock
        std::unique_lock<std::mutex> cv_lk(m_MutexScheduleFinalBlockConsensus);
        if (cv_scheduleFinalBlockConsensus.wait_for(
                cv_lk, std::chrono::seconds(MICROBLOCK_TIMEOUT))
            == std::cv_status::timeout)
        {
            LOG_GENERAL(
                WARNING,
                "Timeout: Didn't receive all Microblock. Proceeds without it");

            auto func
                = [this]() mutable -> void { RunConsensusOnFinalBlock(); };

            DetachedFunction(1, func);
        }
    }
    else
    {
        // The oldest DS committee member will be a shard node at this point -> need to set myself up as a shard node

        // I need to know my shard ID -> iterate through m_shards
        bool found = false;
        for (unsigned int i = 0; i < m_shards.size() && !found; i++)
        {
            for (const auto& j : m_shards.at(i))
            {
                if (j.first == m_mediator.m_selfKey.second)
                {
                    m_mediator.m_node->SetMyShardID(i);
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "WARNING: Oldest DS node not in any of the new shards!");
            return;
        }

        // Populate my sharding structure and txn sharing assignments as a shard node

        unsigned int cur_offset
            = m_mediator.m_dsBlockChain.GetLastBlock().GetSerializedSize()
            + (IP_SIZE + PORT_SIZE);

        // [Sharding structure] -> Use the loading function for shard node
        if (m_mediator.m_node->LoadShardingStructure(m_PoWConsensusMessage,
                                                     cur_offset)
            == false)
        {
            return;
        }

        // [Txn sharing assignments] -> Use the loading function for shard node
        m_mediator.m_node->LoadTxnSharingInfo(m_PoWConsensusMessage,
                                              cur_offset);

        // Finally, start as a shard node
        m_mediator.m_node->StartFirstTxEpoch();
    }
}

void DirectoryService::ProcessDSBlockConsensusWhenDone(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DS block consensus is DONE!!!");

    if (m_mode == PRIMARY_DS)
    {
        LOG_STATE("[DSCON][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "]["
                             << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                      + 1 << "] DONE");
    }

    {
        lock_guard<mutex> g(m_mutexPendingDSBlock);

        if (m_pendingDSBlock == nullptr)
        {
            LOG_GENERAL(FATAL,
                        "assertion failed (" << __FILE__ << ":" << __LINE__
                                             << ": " << __FUNCTION__ << ")");
        }

        // Update the DS Block with the co-signatures from the consensus
        m_pendingDSBlock->SetCoSignatures(*m_consensusObject);

        // Re-serialize the DS Block part of the cached DS Block consensus announcement message so we get the updated co-signatures
        m_pendingDSBlock->Serialize(m_PoWConsensusMessage, 0);

        if (m_pendingDSBlock->GetHeader().GetBlockNum()
            > m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()
                + 1)
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
        SendDSBlockToLookupNodes();
    }

    // Let's reuse the same DS nodes to send the DS Block to the new DS leader
    // Why is this done separately?  Because the new DS leader is not part of m_shards.
    // In multicast code below, we use m_shards as the basis for sending to all the shard nodes.

    if (m_consensusMyID > nodeToSendToLookUpLo
        && m_consensusMyID < nodeToSendToLookUpHi)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I the DS folks that will soon be sending the DSBlock to the "
                  "new DS leader");
        SendDSBlockToNewDSLeader(winnerpeer);
    }

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "New DSBlock created with chosen nonce   = 0x"
            << hex << "\n"
            << m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetNonce()
            << "\n"
            << "New DSBlock hash is                     = 0x"
            << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand) << "\n"
            << "New DS leader (PoW winner)          = " << winnerpeer);

    unsigned int my_DS_cluster_num, my_shards_lo, my_shards_hi;
    SetupMulticastConfigForDSBlock(my_DS_cluster_num, my_shards_lo,
                                   my_shards_hi);

    LOG_STATE(
        "[DSBLK]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] BEFORE SENDING DSBLOCK");

    // Too few target nodes - avoid asking all DS clusters to send
    if ((my_DS_cluster_num + 1) <= m_shards.size())
    {
        SendDSBlockToShardNodes(my_shards_lo, my_shards_hi);
    }

    LOG_STATE(
        "[DSBLK]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] AFTER SENDING DSBLOCK");

    UpdateMyDSModeAndConsensusId();

    UpdateDSCommiteeComposition(winnerpeer);

    StartFirstTxEpoch();
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessDSBlockConsensus(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();
    // Consensus messages must be processed in correct sequence as they come in
    // It is possible for ANNOUNCE to arrive before correct DS state
    // In that case, ANNOUNCE will sleep for a second below
    // If COLLECTIVESIG also comes in, it's then possible COLLECTIVESIG will be processed before ANNOUNCE!
    // So, ANNOUNCE should acquire a lock here
    {
        lock_guard<mutex> g(m_mutexConsensus);

        // Wait until ProcessDSBlock in the case that primary sent announcement pretty early
        if ((m_state == POW_SUBMISSION) || (m_state == DSBLOCK_CONSENSUS_PREP)
            || (m_state == VIEWCHANGE_CONSENSUS))
        {
            cv_DSBlockConsensus.notify_all();

            std::unique_lock<std::mutex> cv_lk(m_MutexCVDSBlockConsensusObject);

            if (cv_DSBlockConsensusObject.wait_for(
                    cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT))
                == std::cv_status::timeout)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Time out while waiting for state transition and "
                          "consensus object creation ");
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "State transition is completed and consensus object "
                      "creation. (check for timeout)");
        }

        if (!CheckState(PROCESS_DSBLOCKCONSENSUS))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Ignoring consensus message");
            return false;
        }
    }

    // Consensus messages must be processed in correct sequence as they come in
    // It is possible for ANNOUNCE to arrive before correct DS state
    // In that case, state transition will occurs and ANNOUNCE will be processed.

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
                                "m_consensusObject is a nullptr. It has not "
                                "been initialized.")
                    return false;
                }
                return m_consensusObject->CanProcessMessage(message, offset);
            }))
    {
        // Correct order preserved
    }
    else
    {
        LOG_GENERAL(
            WARNING,
            "Timeout while waiting for correct order of DS Block consensus "
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
        m_viewChangeCounter = 0;
        cv_viewChangeDSBlock.notify_all();
        ProcessDSBlockConsensusWhenDone(message, offset);
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "No consensus reached. Wait for view change");
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DEBUG for verify sig m_allPoWConns  size is "
                << m_allPoWConns.size()
                << ". Please check numbers of pow receivied by this node");
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
