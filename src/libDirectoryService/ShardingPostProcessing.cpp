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
bool DirectoryService::SendEntireShardingStructureToLookupNodes()
{
    vector<unsigned char> sharding_message
        = {MessageType::LOOKUP, LookupInstructionType::ENTIRESHARDINGSTRUCTURE};
    unsigned int curr_offset = MessageOffset::BODY;

    ShardingStructure::Serialize(m_shards, sharding_message, curr_offset);

    m_mediator.m_lookup->SendMessageToLookupNodes(sharding_message);

    return true;
}

void DirectoryService::SetupMulticastConfigForShardingStructure(
    unsigned int& my_DS_cluster_num, unsigned int& my_shards_lo,
    unsigned int& my_shards_hi)
{

    // Message = [4-byte shard ID] [4-byte committee size] [33-byte public key] [16-byte ip] [4-byte port] ... (all nodes; first entry is leader)

    // Multicast assignments:
    // 1. Divide DS committee into clusters of size 20
    // 2. Each cluster talks to all shard members in each shard
    //    DS cluster 0 => Shard 0
    //    DS cluster 1 => Shard 1
    //    ...
    //    DS cluster 0 => Shard (num of DS clusters)
    //    DS cluster 1 => Shard (num of DS clusters + 1)

    LOG_MARKER();

    unsigned int num_DS_clusters
        = m_mediator.m_DSCommittee.size() / DS_MULTICAST_CLUSTER_SIZE;
    if ((m_mediator.m_DSCommittee.size() % DS_MULTICAST_CLUSTER_SIZE) > 0)
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

void DirectoryService::SendEntireShardingStructureToShardNodes(
    unsigned int my_shards_lo, unsigned int my_shards_hi)
{
    LOG_MARKER();

    // Message = [8-byte DS blocknum] [4-byte shard ID] [Sharding structure] [Txn sharing assignments]
    vector<unsigned char> sharding_message
        = {MessageType::NODE, NodeInstructionType::SHARDING};
    unsigned int curr_offset = MessageOffset::BODY;

    // [8-byte DS blocknum]
    Serializable::SetNumber<uint64_t>(
        sharding_message, curr_offset,
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
        sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    // [4-byte shard ID] -> dummy value at this point; will be updated in loop below
    Serializable::SetNumber<uint32_t>(sharding_message, curr_offset, 0,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // [Sharding structure]
    curr_offset
        = ShardingStructure::Serialize(m_shards, sharding_message, curr_offset);

    // [Txn sharing assignments]
    sharding_message.resize(sharding_message.size()
                            + m_txnSharingMessage.size());
    copy(m_txnSharingMessage.begin(), m_txnSharingMessage.end(),
         sharding_message.begin() + curr_offset);

    auto p = m_shards.begin();
    advance(p, my_shards_lo);
    for (unsigned int i = my_shards_lo; i <= my_shards_hi; i++)
    {
        // [4-byte shard ID] -> get from the leader's info in m_publicKeyToShardIdMap
        Serializable::SetNumber<uint32_t>(
            sharding_message, MessageOffset::BODY + sizeof(uint64_t),
            m_publicKeyToShardIdMap.at(p->begin()->first), sizeof(uint32_t));

        // Send the message
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
        sha256.Update(sharding_message);
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
                                                    sharding_message);
        p++;
    }

    m_txnSharingMessage.clear();
}

#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessShardingConsensus(
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
        // Wait until in the case that primary sent announcement pretty early
        if ((m_state == POW2_SUBMISSION)
            || (m_state == SHARDING_CONSENSUS_PREP))
        {
            cv_shardingConsensus.notify_all();

            std::unique_lock<std::mutex> cv_lk(
                m_MutexCVShardingConsensusObject);

            if (cv_shardingConsensusObject.wait_for(
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

        // if (m_state != SHARDING_CONSENSUS)
        if (!CheckState(PROCESS_SHARDINGCONSENSUS))
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
    // Wait until in the case that primary sent announcement pretty early
    if ((m_state == POW2_SUBMISSION) || (m_state == SHARDING_CONSENSUS_PREP))
    {
        cv_shardingConsensus.notify_all();

        std::unique_lock<std::mutex> cv_lk(m_MutexCVShardingConsensusObject);

        if (cv_shardingConsensusObject.wait_for(
                cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Time out while waiting for state transition and "
                      "consensus object creation ");
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State transition is completed and consensus object "
                  "creation. (check for timeout)");
    }

    if (!CheckState(PROCESS_SHARDINGCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Ignoring consensus message");
        return false;
    }

    if (!m_consensusObject->ProcessMessage(message, offset, from))
    {
        return false;
    }

    ConsensusCommon::State state = m_consensusObject->GetState();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << m_consensusObject->GetStateString());

    if (state == ConsensusCommon::State::DONE)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Sharding consensus is DONE!!!");
        cv_viewChangeSharding.notify_all();

        if (m_mode == PRIMARY_DS)
        {
            LOG_STATE("[SHCON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_txBlockChain.GetLastBlock()
                              .GetHeader()
                              .GetBlockNum()
                          + 1
                      << "] DONE");
        }

        // TODO: Refine this
        unsigned int nodeToSendToLookUpLo = COMM_SIZE / 4;
        unsigned int nodeToSendToLookUpHi
            = nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

        if (m_consensusMyID > nodeToSendToLookUpLo
            && m_consensusMyID < nodeToSendToLookUpHi)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "I the DS folks that will soon be sending the "
                      "sharding structure to the "
                      "lookup nodes");
            SendEntireShardingStructureToLookupNodes();
        }

        unsigned int my_DS_cluster_num, my_shards_lo, my_shards_hi;
        SetupMulticastConfigForShardingStructure(my_DS_cluster_num,
                                                 my_shards_lo, my_shards_hi);

        LOG_STATE("[SHSTU][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "]["
                             << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                      + 1 << "] BEFORE SENDING SHARDING STRUCTURE");

        // Too few target shards - avoid asking all DS clusters to send
        if ((my_DS_cluster_num + 1) <= m_shards.size())
        {
            SendEntireShardingStructureToShardNodes(my_shards_lo, my_shards_hi);
        }

        LOG_STATE("[SHSTU][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "]["
                             << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                      + 1 << "] AFTER SENDING SHARDING STRUCTURE");

        lock_guard<mutex> g(m_mutexAllPOW2);
        m_allPoW2s.clear();
        m_sortedPoW2s.clear();
        m_viewChangeCounter = 0;

        // Start sharding work
        SetState(MICROBLOCK_SUBMISSION);

        // Check for state change. If it get stuck at microblock submission for too long,
        // Move on to finalblock without the microblock
        std::unique_lock<std::mutex> cv_lk(m_MutexScheduleFinalBlockConsensus);
        if (cv_scheduleFinalBlockConsensus.wait_for(
                cv_lk, std::chrono::seconds(SHARDING_TIMEOUT))
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
    else if (state == ConsensusCommon::State::ERROR)
    {
        for (unsigned int i = 0; i < m_mediator.m_DSCommittee.size(); i++)
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                string(
                    m_mediator.m_DSCommittee[i].second.GetPrintableIPAddress())
                    + ":"
                    + to_string(
                          m_mediator.m_DSCommittee[i].second.m_listenPortHost));
        }
        for (unsigned int i = 0; i < m_mediator.m_DSCommittee.size(); i++)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      DataConversion::SerializableToHexStr(
                          m_mediator.m_DSCommittee[i].first));
        }
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "No consensus reached. Wait for view change");
        return false;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << state);

        cv_processConsensusMessage.notify_all();
    }

#endif // IS_LOOKUP_NODE
    return true;
}
