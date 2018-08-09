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
    // Multicast VC block to my assigned shard's nodes - send VCBLOCK message
    // Message = [VC block]

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
        = m_mediator.m_DSCommittee->size() / DS_MULTICAST_CLUSTER_SIZE;
    if ((m_mediator.m_DSCommittee->size() % DS_MULTICAST_CLUSTER_SIZE) > 0)
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
                shard_peers.emplace_back(kv.second);
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
    for (auto const& kv : *m_mediator.m_DSCommittee)
    {
        if (m_pendingVCBlock->GetB2().at(index) == true)
        {
            keys.emplace_back(kv.first);
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

    Peer expectedLeader;
    if (m_mediator.m_DSCommittee->at(m_viewChangeCounter).second == Peer())
    {
        // I am 0.0.0.0
        expectedLeader = m_mediator.m_selfPeer;
    }
    else
    {
        expectedLeader
            = m_mediator.m_DSCommittee->at(m_viewChangeCounter).second;
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

        // Kick ousted leader and/or faulty candidate leader to the back of the queue, waiting to be eject at
        // the next ds epoch
        // Suppose X1 - X3 are faulty ds nodes
        // [X1][X2][X3][N1][N2]
        // View change happen 3 times
        // The view change will restructure the ds committee into this
        // [X2][X3][N1][N2][X1] first vc. vc counter = 1
        // [X3][N1][N2][X1][X2] second vc. vc counter = 2
        // [N1][N2][X1][X2][X3] third vc. vc counter = 3
        // X1 m_consensusMyID = (size of ds committee) - 1 - faultyLeaderIndex (original)
        //                    = 5 - 1 - 0 = 2
        // X2 m_consensusMyID = (size of ds committee) - 1 - faultyLeaderIndex (original)
        //                    = 5 - 1 - 1 = 3
        // X3 m_consensusMyID = (size of ds committee) - 1 - faultyLeaderIndex (original)
        //                    = 5 - 1 - 2 = 4

        bool isCurrentNodeFaulty = false;
        {
            lock_guard<mutex> g2(m_mediator.m_mutexDSCommittee);

            for (unsigned int faultyLeaderIndex = 0;
                 faultyLeaderIndex < m_viewChangeCounter; faultyLeaderIndex++)
            {
                LOG_GENERAL(INFO,
                            "Ejecting "
                                << m_mediator.m_DSCommittee->front().second);

                // Adjust ds commiteee
                m_mediator.m_DSCommittee->push_back(
                    m_mediator.m_DSCommittee->front());
                m_mediator.m_DSCommittee->pop_front();

                // Adjust faulty DS leader and/or faulty ds candidate leader
                if (m_consensusMyID == faultyLeaderIndex)
                {
                    if (m_consensusMyID == 0)
                    {
                        LOG_EPOCH(
                            INFO,
                            to_string(m_mediator.m_currentEpochNum).c_str(),
                            "Current node is a faulty DS leader and got ousted "
                            "by the DS "
                            "Committee");
                    }
                    else
                    {
                        LOG_EPOCH(
                            INFO,
                            to_string(m_mediator.m_currentEpochNum).c_str(),
                            "Current node is a faulty DS candidate leader and "
                            "got ousted by the DS "
                            "Committee");
                    }

                    // calculate (new) my consensus id for faulty ds nodes.
                    // Good ds nodes adjustment have already been done previously.
                    // m_consensusMyID = last index - num of time vc occur + faulty index
                    // Need to add +1 at the end as vc counter begin at 1.
                    m_consensusMyID = (m_mediator.m_DSCommittee->size() - 1)
                        - m_viewChangeCounter + faultyLeaderIndex + 1;
                    isCurrentNodeFaulty = true;
                    LOG_GENERAL(INFO,
                                "new m_consensusMyID  is " << m_consensusMyID);
                }
            }

            // Faulty node already adjusted. Hence, only adjust current node here if it is not faultu.
            if (!isCurrentNodeFaulty)
            {
                LOG_GENERAL(INFO, "Old m_consensusMyID " << m_consensusMyID);
                m_consensusMyID -= m_viewChangeCounter;
                LOG_GENERAL(INFO, "New m_consensusMyID " << m_consensusMyID);
            }
        }

        LOG_GENERAL(INFO, "New view of ds committee: ");
        for (unsigned i = 0; i < m_mediator.m_DSCommittee->size(); i++)
        {
            LOG_GENERAL(INFO, m_mediator.m_DSCommittee->at(i).second);
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
    {
        vector<Peer> allPowSubmitter;
        for (auto& nodeNetwork : m_allPoWConns)
        {
            allPowSubmitter.emplace_back(nodeNetwork.second);
        }
        P2PComm::GetInstance().SendBroadcastMessage(allPowSubmitter,
                                                    vcblock_message);
        break;
    }
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
    {
        DetermineShardsToSendVCBlockTo(my_DS_cluster_num, my_shards_lo,
                                       my_shards_hi);
        SendVCBlockToShardNodes(my_DS_cluster_num, my_shards_lo, my_shards_hi,
                                vcblock_message);
        break;
    }
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
    default:
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "illegal view change state. state: " << to_string(viewChangeState));
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
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Re-running finalblock consensus");
        RunConsensusOnFinalBlock();
        break;
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
    default:
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "illegal view change state. state: " << to_string(viewChangeState));
    }
}

#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessViewChangeConsensus(
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

        std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeConsensusObj);
        if (cv_ViewChangeConsensusObj.wait_for(
                cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
                [this] { return (m_state == VIEWCHANGE_CONSENSUS); }))
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Successfully transit to viewchange consensus or I am in the "
                "correct state.");
        }
        else
        {
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Time out while waiting for state transition to view change "
                "consensus and "
                "consensus object creation. Most likely view change didn't "
                "occur. A malicious node may be trying to initate view "
                "change.");
        }

        if (!CheckState(PROCESS_VIEWCHANGECONSENSUS))
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Ignoring consensus message. Not at viewchange consensus "
                      "state.");
            return false;
        }
    }

    // Consensus messages must be processed in correct sequence as they come in
    // It is possible for ANNOUNCE to arrive before correct DS state
    // In that case, state transition will occurs and ANNOUNCE will be processed.
    std::unique_lock<mutex> cv_lk_con_msg(m_mutexProcessConsensusMessage);
    if (cv_processConsensusMessage.wait_for(
            cv_lk_con_msg,
            std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
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
        LOG_GENERAL(WARNING,
                    "Timeout while waiting for correct order of View Change "
                    "Block consensus "
                    "messages");
        return false;
    }

    lock_guard<mutex> g(m_mutexConsensus);

    if (!m_consensusObject->ProcessMessage(message, offset, from))
    {
        return false;
    }

    ConsensusCommon::State state = m_consensusObject->GetState();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << m_consensusObject->GetStateString());

    if (state == ConsensusCommon::State::DONE)
    {
        cv_ViewChangeVCBlock.notify_all();
        ProcessViewChangeConsensusWhenDone();
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "View change consensus is DONE!!!");
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "No consensus reached. Will attempt to do view change again");
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
