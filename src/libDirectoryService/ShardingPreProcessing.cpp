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
void DirectoryService::ComputeSharding()
{
    LOG_MARKER();

    m_shards.clear();
    m_publicKeyToShardIdMap.clear();

    uint32_t numOfComms = m_allPoW2s.size() / COMM_SIZE;

    if (numOfComms == 0)
    {
        LOG_GENERAL(WARNING,
                    "Zero Pow2 collected, numOfComms is temporarlly set to 1");
        numOfComms = 1;
    }

    for (unsigned int i = 0; i < numOfComms; i++)
    {
        m_shards.emplace_back();
    }

    for (auto& kv : m_allPoW2s)
    {
        PubKey key = kv.first;
        uint256_t nonce = kv.second;
        // sort all PoW2 submissions according to H(nonce, pubkey)
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> hashVec;
        hashVec.resize(POW_SIZE + PUB_KEY_SIZE);
        Serializable::SetNumber<uint256_t>(hashVec, 0, nonce, UINT256_SIZE);
        key.Serialize(hashVec, POW_SIZE);
        sha2.Update(hashVec);
        const vector<unsigned char>& sortHashVec = sha2.Finalize();
        array<unsigned char, BLOCK_HASH_SIZE> sortHash;
        copy(sortHashVec.begin(), sortHashVec.end(), sortHash.begin());
        m_sortedPoW2s.emplace(sortHash, key);
    }

    lock_guard<mutex> g(m_mutexAllPoWConns, adopt_lock);
    unsigned int i = 0;
    for (auto& kv : m_sortedPoW2s)
    {
        PubKey key = kv.second;
        map<PubKey, Peer>& shard = m_shards.at(i % numOfComms);
        shard.emplace(key, m_allPoWConns.at(key));
        m_publicKeyToShardIdMap.emplace(key, i % numOfComms);
        i++;
    }
}

void DirectoryService::ComputeTxnSharingAssignments()
{
    LOG_MARKER();

    // PART 1
    // First version: We just take the first X nodes in DS committee

    m_DSReceivers.clear();

    LOG_GENERAL(INFO,
                "debug " << m_mediator.m_DSCommittee.size() << " "
                         << TX_SHARING_CLUSTER_SIZE);

    uint32_t num_ds_nodes
        = (m_mediator.m_DSCommittee.size() < TX_SHARING_CLUSTER_SIZE)
        ? m_mediator.m_DSCommittee.size()
        : TX_SHARING_CLUSTER_SIZE;

    for (unsigned int i = 0; i < num_ds_nodes; i++)
    {
        if (i != m_consensusMyID)
        {
            m_DSReceivers.emplace_back(m_mediator.m_DSCommittee.at(i).second);
        }
        else
        {
            // when i == m_consensusMyID use m_mediator.m_selfPeer since IP/ port in m_mediator.m_DSCommittee.at(m_consensusMyID).second is zeroed out
            m_DSReceivers.emplace_back(m_mediator.m_selfPeer);
        }
    }

    // For this version, DS leader (the one invoking this function) is part of the X nodes to receive and share Tx bodies -> fill up leader's assigned nodes

    m_sharingAssignment.clear();

    for (unsigned int i = num_ds_nodes; i < m_mediator.m_DSCommittee.size();
         i++)
    {
        m_sharingAssignment.emplace_back(m_mediator.m_DSCommittee.at(i).second);
    }

    // PART 2 and 3
    // First version: We just take the first X nodes for receiving and next X nodes for sending

    m_shardReceivers.clear();
    m_shardSenders.clear();

    for (unsigned int i = 0; i < m_shards.size(); i++)
    {
        const map<PubKey, Peer>& shard = m_shards.at(i);

        // PART 2

        m_shardReceivers.emplace_back();

        uint32_t nodes_recv_lo = 0;
        uint32_t nodes_recv_hi = nodes_recv_lo + TX_SHARING_CLUSTER_SIZE - 1;

        if (nodes_recv_hi >= shard.size())
        {
            nodes_recv_hi = shard.size() - 1;
        }

        unsigned int num_nodes = nodes_recv_hi - nodes_recv_lo + 1;

        auto node_peer = shard.begin();
        for (unsigned int j = 0; j < num_nodes; j++)
        {
            m_shardReceivers.back().emplace_back(node_peer->second);
            node_peer++;
        }

        // PART 3

        m_shardSenders.emplace_back();

        uint32_t nodes_send_lo = 0;
        uint32_t nodes_send_hi = 0;

        if (shard.size() <= TX_SHARING_CLUSTER_SIZE)
        {
            nodes_send_lo = nodes_recv_lo;
            nodes_send_hi = nodes_recv_hi;
        }
        else if (shard.size() < (2 * TX_SHARING_CLUSTER_SIZE))
        {
            nodes_send_lo = shard.size() - TX_SHARING_CLUSTER_SIZE;
            nodes_send_hi = nodes_send_lo + TX_SHARING_CLUSTER_SIZE - 1;
        }
        else
        {
            nodes_send_lo = TX_SHARING_CLUSTER_SIZE;
            nodes_send_hi = nodes_send_lo + TX_SHARING_CLUSTER_SIZE - 1;
        }

        num_nodes = nodes_send_hi - nodes_send_lo + 1;

        node_peer = shard.begin();
        advance(node_peer, nodes_send_lo);

        for (unsigned int j = 0; j < num_nodes; j++)
        {
            m_shardSenders.back().emplace_back(node_peer->second);
            node_peer++;
        }
    }
}

bool DirectoryService::RunConsensusOnShardingWhenDSPrimary()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the leader DS node. Creating sharding structure.");

    // Aggregate validated PoW2 submissions into m_allPoWs and m_allPoWConns
    // I guess only the leader has to do this

    vector<unsigned char> sharding_structure;

    ComputeSharding();
    unsigned int txn_sharing_offset
        = ShardingStructure::Serialize(m_shards, sharding_structure, 0);

    ComputeTxnSharingAssignments();
    TxnSharingAssignments::Serialize(m_DSReceivers, m_shardReceivers,
                                     m_shardSenders, sharding_structure,
                                     txn_sharing_offset);

    // Save the raw transaction body sharing assignment message for propagating later to shard nodes
    m_txnSharingMessage.resize(sharding_structure.size() - txn_sharing_offset);
    copy(sharding_structure.begin() + txn_sharing_offset,
         sharding_structure.end(), m_txnSharingMessage.begin());

    // kill first ds leader (used for view change testing)
    /**
    if (m_consensusMyID == 0 && m_viewChangeCounter < 1)
    {
        LOG_GENERAL(INFO, "I am killing myself to test view change");
        throw exception();
    }
    **/
    // Create new consensus object

    // Dummy values for now
    uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    m_consensusObject.reset(new ConsensusLeader(
        consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(SHARDINGCONSENSUS),
        std::function<bool(const vector<unsigned char>&, unsigned int,
                           const Peer&)>(),
        std::function<bool(map<unsigned int, std::vector<unsigned char>>)>()));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << LEADER_SHARDING_PREPARATION_IN_SECONDS
                         << " seconds before announcing...");
    this_thread::sleep_for(
        chrono::seconds(LEADER_SHARDING_PREPARATION_IN_SECONDS));

    LOG_STATE(
        "[SHCON]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] BGIN");

    cl->StartConsensus(sharding_structure, sharding_structure.size());

    return true;
}

void DirectoryService::SaveTxnBodySharingAssignment(
    const vector<unsigned char>& sharding_structure, unsigned int curr_offset)
{
    m_DSReceivers.clear();
    m_shardReceivers.clear();
    m_shardSenders.clear();

    TxnSharingAssignments::Deserialize(sharding_structure, curr_offset,
                                       m_DSReceivers, m_shardReceivers,
                                       m_shardSenders);

    bool i_am_forwarder = false;
    for (uint32_t i = 0; i < m_DSReceivers.size(); i++)
    {
        if (m_DSReceivers.at(i) == m_mediator.m_selfPeer)
        {
            i_am_forwarder = true;
        }
    }

    unsigned int num_ds_nodes = m_DSReceivers.size();

    m_sharingAssignment.clear();

    if ((i_am_forwarder == true)
        && (m_mediator.m_DSCommittee.size() > num_ds_nodes))
    {
        for (unsigned int i = 0; i < m_mediator.m_DSCommittee.size(); i++)
        {
            bool is_a_receiver = false;

            if (num_ds_nodes > 0)
            {
                for (unsigned int j = 0; j < m_DSReceivers.size(); j++)
                {
                    if (m_mediator.m_DSCommittee.at(i).second
                        == m_DSReceivers.at(j))
                    {
                        is_a_receiver = true;
                        break;
                    }
                }
                num_ds_nodes--;
            }

            if (is_a_receiver == false)
            {
                m_sharingAssignment.emplace_back(
                    m_mediator.m_DSCommittee.at(i).second);
            }
        }
    }
}

bool DirectoryService::ShardingValidator(
    const vector<unsigned char>& sharding_structure,
    [[gnu::unused]] std::vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    m_shards.clear();
    m_publicKeyToShardIdMap.clear();

    lock_guard<mutex> g(m_mutexAllPoWConns);

    unsigned int curr_offset = 0;

    curr_offset = ShardingStructure::Deserialize(sharding_structure,
                                                 curr_offset, m_shards);

    for (unsigned int i = 0; i < m_shards.size(); i++)
    {
        for (auto& j : m_shards.at(i))
        {
            auto storedMember = m_allPoWConns.find(j.first);

            // I know the member but the member IP given by the leader is different!
            if (storedMember != m_allPoWConns.end())
            {
                if (storedMember->second != j.second)
                {
                    LOG_EPOCH(WARNING,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "WARNING: Why is the IP of the member different "
                              "from what I have in m_allPoWConns???");
                    return false;
                }
            }
            // I don't know the member -> store the IP given by the leader
            else
            {
                m_allPoWConns.emplace(j.first, j.second);
            }

            m_publicKeyToShardIdMap.emplace(j.first, i);
        }
    }

    SaveTxnBodySharingAssignment(sharding_structure, curr_offset);

    // Save the raw transaction body sharing assignment message for propagating later to shard nodes
    m_txnSharingMessage.resize(sharding_structure.size() - curr_offset);
    copy(sharding_structure.begin() + curr_offset, sharding_structure.end(),
         m_txnSharingMessage.begin());

    return true;
}

bool DirectoryService::RunConsensusOnShardingWhenDSBackup()
{
    LOG_MARKER();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am a backup DS node. Waiting for sharding structure announcement.");

    // Create new consensus object

    // Dummy values for now
    uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return ShardingValidator(message, errorMsg);
    };

    m_consensusObject.reset(new ConsensusBackup(
        consensusID, m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(SHARDINGCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

void DirectoryService::RunConsensusOnSharding()
{
    LOG_MARKER();
    SetState(SHARDING_CONSENSUS_PREP);

    lock_guard<mutex> g(m_mutexAllPOW2);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Num of PoW2 sub rec: " << m_allPoW2s.size());
    LOG_STATE("[POW2R][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_allPoW2s.size() << "] ");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "My consensus id is " << m_consensusMyID);

    if (m_allPoW2s.size() == 0)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "To-do: Code up the logic for if we didn't get any "
                  "submissions at all");
        // throw exception();
        return;
    }

    if (m_mode == PRIMARY_DS)
    {
        if (!RunConsensusOnShardingWhenDSPrimary())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error encountered with running sharding consensus "
                      "as ds leader");
            // throw exception();
            return;
        }
    }
    else
    {
        if (!RunConsensusOnShardingWhenDSBackup())
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error encountered with running sharding consensus "
                      "as ds backup")
            // throw exception();
            return;
        }
    }

    SetState(SHARDING_CONSENSUS);
    cv_shardingConsensusObject.notify_all();

    // View change will wait for timeout. If conditional variable is notified before timeout, the thread will return
    // without triggering view change.
    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeSharding);
    if (cv_viewChangeSharding.wait_for(cv_lk,
                                       std::chrono::seconds(VIEWCHANGE_TIME))
        == std::cv_status::timeout)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Initiated sharding structure consensus view change. ");
        auto func = [this]() -> void { RunConsensusOnViewChange(); };
        DetachedFunction(1, func);
    }
}
#endif // IS_LOOKUP_NODE
