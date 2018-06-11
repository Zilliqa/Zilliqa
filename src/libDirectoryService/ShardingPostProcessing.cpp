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
unsigned int DirectoryService::SerializeEntireShardingStructure(
    vector<unsigned char>& sharding_message, unsigned int curr_offset)
{
    LOG_MARKER();

    // 4-byte number of shards
    Serializable::SetNumber<uint32_t>(sharding_message, curr_offset,
                                      m_shards.size(), sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of shards = " << m_shards.size());

    for (auto it_vec = m_shards.begin(); it_vec != m_shards.end(); it_vec++)
    {
        // 4-byte shard size
        Serializable::SetNumber<uint32_t>(sharding_message, curr_offset,
                                          it_vec->size(), sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Shard size = " << it_vec->size());

        for (auto it_map = it_vec->begin(); it_map != it_vec->end(); it_map++)
        {
            // 33-byte public key
            (*it_map).first.Serialize(sharding_message, curr_offset);
            curr_offset += PUB_KEY_SIZE;

            // 16-byte ip + 4-byte port
            (*it_map).second.Serialize(sharding_message, curr_offset);
            curr_offset += IP_SIZE + PORT_SIZE;

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "PubKey: "
                          << DataConversion::SerializableToHexStr(
                                 (*it_map).first)
                          << " IP: " << (*it_map).second.GetPrintableIPAddress()
                          << " Port: " << (*it_map).second.m_listenPortHost);
        }
    }

    return true;
}

bool DirectoryService::SendEntireShardingStructureToLookupNodes()
{
    vector<unsigned char> sharding_message
        = {MessageType::LOOKUP, LookupInstructionType::ENTIRESHARDINGSTRUCTURE};
    unsigned int curr_offset = MessageOffset::BODY;

    // Set view change count
    Serializable::SetNumber<unsigned int>(sharding_message, curr_offset,
                                          m_viewChangeCounter,
                                          sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    SerializeEntireShardingStructure(sharding_message, curr_offset);

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

    unsigned int num_DS_clusters = m_mediator.m_DSCommitteeNetworkInfo.size()
        / DS_MULTICAST_CLUSTER_SIZE;
    if ((m_mediator.m_DSCommitteeNetworkInfo.size() % DS_MULTICAST_CLUSTER_SIZE)
        > 0)
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

void DirectoryService::SendingShardingStructureToShard(
    vector<std::map<PubKey, Peer>>::iterator& p)
{
    LOG_MARKER();

    // Message = [32-byte DS blocknum] [4-byte shard ID] [4-byte committee size] [33-byte public key]
    // [16-byte ip] [4-byte port] ... (all nodes; first entry is leader)
    vector<unsigned char> sharding_message
        = {MessageType::NODE, NodeInstructionType::SHARDING};
    unsigned int curr_offset = MessageOffset::BODY;

    // Todo: Any better way to do it?
    uint256_t latest_block_num_in_blockchain
        = m_mediator.m_dsBlockChain.GetBlockCount() - 1;

    // todo: Relook at this. This is not secure
    Serializable::SetNumber<unsigned int>(sharding_message, curr_offset,
                                          m_viewChangeCounter,
                                          sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    Serializable::SetNumber<uint256_t>(sharding_message, curr_offset,
                                       latest_block_num_in_blockchain,
                                       UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    // 4-byte shard ID - get from the leader's info in m_publicKeyToShardIdMap
    Serializable::SetNumber<uint32_t>(
        sharding_message, curr_offset,
        m_publicKeyToShardIdMap.at(p->begin()->first), sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 4-byte number of shards
    Serializable::SetNumber<uint32_t>(sharding_message, curr_offset,
                                      m_shards.size(), sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 4-byte committee size
    Serializable::SetNumber<uint32_t>(sharding_message, curr_offset, p->size(),
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Committee size = " << p->size());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Members:");

    vector<Peer> shard_peers;
    for (auto& kv : *p)
    {
        // 33-byte public key
        kv.first.Serialize(sharding_message, curr_offset);
        curr_offset += PUB_KEY_SIZE;
        // 16-byte ip + 4-byte port

        kv.second.Serialize(sharding_message, curr_offset);
        curr_offset += IP_SIZE + PORT_SIZE;
        shard_peers.push_back(kv.second);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  " PubKey: " << DataConversion::SerializableToHexStr(kv.first)
                              << " IP: " << kv.second.GetPrintableIPAddress()
                              << " Port: " << kv.second.m_listenPortHost);
    }

#ifdef STAT_TEST
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(sharding_message);
    vector<unsigned char> this_msg_hash = sha256.Finalize();

    LOG_STATE(
        "[INFOR]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6) << "]["
        << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
               .substr(0, 6)
        << "][" << m_mediator.m_txBlockChain.GetBlockCount() << "] SHMSG");
#endif // STAT_TEST

    P2PComm::GetInstance().SendBroadcastMessage(shard_peers, sharding_message);
    p++;
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessShardingConsensus(
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

    // if (m_state != SHARDING_CONSENSUS)
    if (!CheckState(PROCESS_SHARDINGCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Ignoring consensus message");
        return false;
    }

    bool result = m_consensusObject->ProcessMessage(message, offset, from);
    ConsensusCommon::State state = m_consensusObject->GetState();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << state);

    if (state == ConsensusCommon::State::DONE)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Sharding consensus is DONE!!!");
        cv_viewChangeSharding.notify_all();

#ifdef STAT_TEST
        if (m_mode == PRIMARY_DS)
        {
            LOG_STATE("[SHCON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_txBlockChain.GetBlockCount() << "] DONE");
        }
#endif // STAT_TEST

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

        // Too few target shards - avoid asking all DS clusters to send
        if ((my_DS_cluster_num + 1) <= m_shards.size())
        {
            auto p = m_shards.begin();
            advance(p, my_shards_lo);
            for (unsigned int i = my_shards_lo; i <= my_shards_hi; i++)
            {
                SendingShardingStructureToShard(p);
            }
        }

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
            RunConsensusOnFinalBlock();
        }
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        for (unsigned int i = 0; i < m_mediator.m_DSCommitteeNetworkInfo.size();
             i++)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      string(m_mediator.m_DSCommitteeNetworkInfo[i]
                                 .GetPrintableIPAddress())
                          + ":"
                          + to_string(m_mediator.m_DSCommitteeNetworkInfo[i]
                                          .m_listenPortHost));
        }
        for (unsigned int i = 0; i < m_mediator.m_DSCommitteePubKeys.size();
             i++)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      DataConversion::SerializableToHexStr(
                          m_mediator.m_DSCommitteePubKeys[i]));
        }
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");
        // throw exception();
        // TODO: no consensus reached
        if (m_mode != PRIMARY_DS)
        {
            RejoinAsDS();
        }
        return false;
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}