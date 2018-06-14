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
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

#ifndef IS_LOOKUP_NODE

void DirectoryService::DetermineShardsToSendVCBlockTo(
    unsigned int& my_DS_cluster_num, unsigned int& my_shards_lo,
    unsigned int& my_shards_hi) const
{
    // Multicast final block to my assigned shard's nodes - send FINALBLOCK message
    // Message = [Final block]

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
        num_DS_clusters++;
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG num of ds clusters " << num_DS_clusters)
    unsigned int shard_groups_count = m_shards.size() / num_DS_clusters;
    if ((m_shards.size() % num_DS_clusters) > 0)
    {
        shard_groups_count++;
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG num of shard group count " << shard_groups_count)

    my_DS_cluster_num = m_consensusMyID / DS_MULTICAST_CLUSTER_SIZE;
    my_shards_lo = my_DS_cluster_num * shard_groups_count;
    my_shards_hi = my_shards_lo + shard_groups_count - 1;

    if (my_shards_hi >= m_shards.size())
    {
        my_shards_hi = m_shards.size() - 1;
    }
}

void DirectoryService::SendVCBlockToShardNodes(
    unsigned int my_DS_cluster_num, unsigned int my_shards_lo,
    unsigned int my_shards_hi, vector<unsigned char>& vcblock_message)
{
    // Too few target shards - avoid asking all DS clusters to send
    LOG_MARKER();

    if ((my_DS_cluster_num + 1) <= m_shards.size())
    {
        auto p = m_shards.begin();
        advance(p, my_shards_lo);

        for (unsigned int i = my_shards_lo; i <= my_shards_hi; i++)
        {
            vector<Peer> shard_peers;

            for (auto& kv : *p)
            {
                shard_peers.push_back(kv.second);
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          " PubKey: "
                              << DataConversion::SerializableToHexStr(kv.first)
                              << " IP: " << kv.second.GetPrintableIPAddress()
                              << " Port: " << kv.second.m_listenPortHost);
            }

            P2PComm::GetInstance().SendBroadcastMessage(shard_peers,
                                                        vcblock_message);
            p++;
        }
    }
}

void DirectoryService::ProcessViewChangeConsensusWhenDone()
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "View change consensus is DONE!!!");

    m_pendingVCBlock->SetCoSignatures(*m_consensusObject);

    unsigned int index = 0;
    unsigned int count = 0;

    vector<PubKey> keys;
    for (auto& kv : m_mediator.m_DSCommitteePubKeys)
    {
        if (m_pendingVCBlock->GetB2().at(index) == true)
        {
            keys.push_back(kv);
            count++;
        }
        index++;
    }

    // Verify cosig against vcblock
    shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
    if (aggregatedKey == nullptr)
    {
        LOG_GENERAL(WARNING, "Aggregated key generation failed");
    }

    vector<unsigned char> message;
    m_pendingVCBlock->GetHeader().Serialize(message, 0);
    m_pendingVCBlock->GetCS1().Serialize(message, VCBlockHeader::SIZE);
    BitVector::SetBitVector(message, VCBlockHeader::SIZE + BLOCK_SIG_SIZE,
                            m_pendingVCBlock->GetB1());
    if (not Schnorr::GetInstance().Verify(message, 0, message.size(),
                                          m_pendingVCBlock->GetCS2(),
                                          *aggregatedKey))
    {
        LOG_GENERAL(WARNING, "cosig verification fail");
        for (auto& kv : keys)
        {
            LOG_GENERAL(WARNING, kv);
        }
        return;
    }

    Peer newLeaderNetworkInfo;
    unsigned char viewChangeState;
    {
        lock_guard<mutex> g(m_mutexPendingVCBlock);

        newLeaderNetworkInfo
            = m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo();
        viewChangeState = m_pendingVCBlock->GetHeader().GetViewChangeState();
    }

    // StoreVCBlockToStorage(); TODO
    unsigned int offsetToCandidateLeader = 1;

    Peer expectedLeader;
    if (m_mediator.m_DSCommitteeNetworkInfo.at(offsetToCandidateLeader)
        == Peer())
    {
        // I am 0.0.0.0
        expectedLeader = m_mediator.m_selfPeer;
    }
    else
    {
        expectedLeader
            = m_mediator.m_DSCommitteeNetworkInfo.at(offsetToCandidateLeader);
    }

    if (expectedLeader == newLeaderNetworkInfo)
    {
        if (newLeaderNetworkInfo == m_mediator.m_selfPeer)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "After view change, I am the new leader!");
            m_mode = PRIMARY_DS;
        }
        else
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "After view change, I am ds backup");
            m_mode = BACKUP_DS;
        }

        // Kick ousted leader to the back of the queue, waiting to be eject at
        // the next ds epoch
        {
            lock(m_mediator.m_mutexDSCommitteeNetworkInfo,
                 m_mediator.m_mutexDSCommitteePubKeys);
            lock_guard<mutex> g2(m_mediator.m_mutexDSCommitteeNetworkInfo,
                                 adopt_lock);
            lock_guard<mutex> g3(m_mediator.m_mutexDSCommitteePubKeys,
                                 adopt_lock);

            m_mediator.m_DSCommitteeNetworkInfo.push_back(
                m_mediator.m_DSCommitteeNetworkInfo.front());
            m_mediator.m_DSCommitteeNetworkInfo.pop_front();

            m_mediator.m_DSCommitteePubKeys.push_back(
                m_mediator.m_DSCommitteePubKeys.front());
            m_mediator.m_DSCommitteePubKeys.pop_front();
        }

        unsigned int offsetTOustedDSLeader = 0;
        if (m_consensusMyID == offsetTOustedDSLeader)
        {
            // Now if I am the ousted leader, I will self-assinged myself to the last
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "I was a DS leader but I got ousted by the DS Committee");
            offsetTOustedDSLeader
                = m_mediator.m_DSCommitteeNetworkInfo.size() - 1;
            m_consensusMyID = offsetTOustedDSLeader;
        }
        else
        {
            m_consensusMyID--;
        }

        auto func = [this, viewChangeState]() -> void {
            ProcessNextConsensus(viewChangeState);
        };
        DetachedFunction(1, func);
    }
    else
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "View change completed but it seems wrong to me."
                      << "expectedLeader: " << expectedLeader
                      << "newLeaderNetworkInfo: " << newLeaderNetworkInfo);
    }

    // TODO: Refine this
    // Broadcasting vcblock to lookup nodes

    vector<unsigned char> vcblock_message
        = {MessageType::NODE, NodeInstructionType::VCBLOCK};
    unsigned int curr_offset = MessageOffset::BODY;

    m_pendingVCBlock->Serialize(vcblock_message, MessageOffset::BODY);
    curr_offset += m_pendingVCBlock->GetSerializedSize();

    unsigned int nodeToSendToLookUpLo = COMM_SIZE / 4;
    unsigned int nodeToSendToLookUpHi
        = nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

    if (m_consensusMyID > nodeToSendToLookUpLo
        && m_consensusMyID < nodeToSendToLookUpHi)
    {
        m_mediator.m_lookup->SendMessageToLookupNodes(vcblock_message);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I the part of the subset of DS committee that have sent the "
                  "VCBlock to the lookup nodes");
    }

    // Broadcasting vcblock to lookup nodes
    unsigned int my_DS_cluster_num;
    unsigned int my_shards_lo;
    unsigned int my_shards_hi;

    switch (viewChangeState)
    {
    case DSBLOCK_CONSENSUS:
    case DSBLOCK_CONSENSUS_PREP:
    case SHARDING_CONSENSUS:
    case SHARDING_CONSENSUS_PREP:
    {
        vector<Peer> allPowSubmitter;
        for (auto& nodeNetwork : m_allPoWConns)
        {
            allPowSubmitter.push_back(nodeNetwork.second);
        }
        P2PComm::GetInstance().SendBroadcastMessage(allPowSubmitter,
                                                    vcblock_message);
        break;
    }
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
    {
        DetermineShardsToSendFinalBlockTo(my_DS_cluster_num, my_shards_lo,
                                          my_shards_hi);
        SendVCBlockToShardNodes(my_DS_cluster_num, my_shards_lo, my_shards_hi,
                                vcblock_message);
        break;
    }
    default:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "illegal view change state. state: " << viewChangeState);
    }
}

void DirectoryService::ProcessNextConsensus(unsigned char viewChangeState)
{
    this_thread::sleep_for(chrono::seconds(POST_VIEWCHANGE_BUFFER));

    switch (viewChangeState)
    {
    case DSBLOCK_CONSENSUS:
    case DSBLOCK_CONSENSUS_PREP:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Re-running dsblock consensus");
        RunConsensusOnDSBlock();
        break;
    case SHARDING_CONSENSUS:
    case SHARDING_CONSENSUS_PREP:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Re-running sharding consensus");
        RunConsensusOnSharding();
        break;
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Re-running finalblock consensus");
        RunConsensusOnFinalBlock();
        break;
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Re-running view change consensus");
        RunConsensusOnViewChange();
    default:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "illegal view change state. state: " << viewChangeState);
    }
}

#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessViewChangeConsensus(
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

    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeConsensusObj);
    if (cv_ViewChangeConsensusObj.wait_for(
            cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
            [this] { return (m_state == VIEWCHANGE_CONSENSUS); }))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Successfully transit to viewchange consensus or I am in the "
                  "correct state.");
    }
    else
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Time out while waiting for state transition to view change "
                  "consensus and "
                  "consensus object creation. Most likely view change didn't "
                  "occur. A malicious node may be trying to initate view "
                  "change.");
    }

    if (!CheckState(PROCESS_VIEWCHANGECONSENSUS))
    {
        LOG_EPOCH(
            WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Ignoring consensus message. Not at viewchange consensus state.");
        return false;
    }

    bool result = m_consensusObject->ProcessMessage(message, offset, from);
    ConsensusCommon::State state = m_consensusObject->GetState();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << state);

    if (state == ConsensusCommon::State::DONE)
    {
        // VC TODO
        cv_ViewChangeVCBlock.notify_all();
        ProcessViewChangeConsensusWhenDone();
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "View change consensus is DONE!!!");
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(
            WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Oops, no consensus reached. Will attempt to do view change again");

        // throw exception();
        // TODO: no consensus reached
        return false;
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}
