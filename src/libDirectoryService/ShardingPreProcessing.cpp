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

void DirectoryService::SerializeShardingStructure(
    vector<unsigned char>& sharding_structure) const
{
    // Sharding structure message format:
    // [4-byte num of committees]
    // [4-byte committee size]
    //   [33-byte public key]
    //   [33-byte public key]
    //   ...
    // [4-byte committee size]
    //   [33-byte public key]
    //   [33-byte public key]
    //   ...

    uint32_t numOfComms = m_shards.size();

    unsigned int curr_offset = 0;

    Serializable::SetNumber<unsigned int>(sharding_structure, curr_offset,
                                          m_viewChangeCounter,
                                          sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    // 4-byte num of committees
    Serializable::SetNumber<uint32_t>(sharding_structure, curr_offset,
                                      numOfComms, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of committees = " << numOfComms);

    for (unsigned int i = 0; i < numOfComms; i++)
    {
        const map<PubKey, Peer>& shard = m_shards.at(i);

        // 4-byte committee size
        Serializable::SetNumber<uint32_t>(sharding_structure, curr_offset,
                                          shard.size(), sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Committee size = " << shard.size() << "\n"
                                      << "Members:");

        for (auto& kv : shard)
        {
            kv.first.Serialize(sharding_structure, curr_offset);
            curr_offset += PUB_KEY_SIZE;

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      " PubKey = "
                          << DataConversion::SerializableToHexStr(kv.first)
                          << " at " << kv.second.GetPrintableIPAddress()
                          << " Port: " << kv.second.m_listenPortHost);
        }
    }
}

void DirectoryService::AppendSharingSetupToShardingStructure(
    vector<unsigned char>& sharding_structure, unsigned int curr_offset)
{
    // Transaction body sharing setup
    // Everyone (DS and non-DS) needs to remember their sharing assignments for this particular block

    // Transaction body sharing assignments:
    // PART 1. Select X random nodes from DS committee for receiving Tx bodies and broadcasting to other DS nodes
    // PART 2. Select X random nodes per shard for receiving Tx bodies and broadcasting to other nodes in the shard
    // PART 3. Select X random nodes per shard for sending Tx bodies to the receiving nodes in other committees (DS and shards)

    // Message format:
    // [4-byte num of DS nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committees]
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // ...

    // PART 1
    // First version: We just take the first X nodes in DS committee
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "debug " << m_mediator.m_DSCommittee.size() << " "
                       << TX_SHARING_CLUSTER_SIZE);

    uint32_t num_ds_nodes
        = (m_mediator.m_DSCommittee.size() < TX_SHARING_CLUSTER_SIZE)
        ? m_mediator.m_DSCommittee.size()
        : TX_SHARING_CLUSTER_SIZE;
    Serializable::SetNumber<uint32_t>(sharding_structure, curr_offset,
                                      num_ds_nodes, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forwarders inside the DS committee (" << num_ds_nodes << "):");

    for (unsigned int i = 0; i < m_consensusMyID; i++)
    {
        m_mediator.m_DSCommittee.at(i).second.Serialize(sharding_structure,
                                                        curr_offset);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  m_mediator.m_DSCommittee.at(i).second);
        curr_offset += IP_SIZE + PORT_SIZE;
    }

    // when i == m_consensusMyID use m_mediator.m_selfPeer since IP/ port in
    // m_mediator.m_DSCommitteeNetworkInfo.at(m_consensusMyID) is zeroed out
    m_mediator.m_selfPeer.Serialize(sharding_structure, curr_offset);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              m_mediator.m_selfPeer);
    curr_offset += IP_SIZE + PORT_SIZE;

    for (unsigned int i = m_consensusMyID + 1; i < num_ds_nodes; i++)
    {
        m_mediator.m_DSCommittee.at(i).second.Serialize(sharding_structure,
                                                        curr_offset);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  m_mediator.m_DSCommittee.at(i).second);
        curr_offset += IP_SIZE + PORT_SIZE;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of shards: " << m_shards.size());

    Serializable::SetNumber<uint32_t>(sharding_structure, curr_offset,
                                      (uint32_t)m_shards.size(),
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // PART 2 and 3
    // First version: We just take the first X nodes for receiving and next X nodes for sending
    for (unsigned int i = 0; i < m_shards.size(); i++)
    {
        const map<PubKey, Peer>& shard = m_shards.at(i);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Shard " << i << " forwarders:");

        // PART 2
        uint32_t nodes_recv_lo = 0;
        uint32_t nodes_recv_hi = nodes_recv_lo + TX_SHARING_CLUSTER_SIZE - 1;
        if (nodes_recv_hi >= shard.size())
        {
            nodes_recv_hi = shard.size() - 1;
        }

        unsigned int num_nodes = nodes_recv_hi - nodes_recv_lo + 1;

        Serializable::SetNumber<uint32_t>(sharding_structure, curr_offset,
                                          num_nodes, sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        map<PubKey, Peer>::const_iterator node_peer = shard.begin();
        for (unsigned int j = 0; j < num_nodes; j++)
        {
            node_peer->second.Serialize(sharding_structure, curr_offset);
            curr_offset += IP_SIZE + PORT_SIZE;

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      node_peer->second);

            node_peer++;
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Shard " << i << " senders:");

        // PART 3
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

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG lo " << nodes_send_lo);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG hi " << nodes_send_hi);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG num_nodes " << num_nodes);

        Serializable::SetNumber<uint32_t>(sharding_structure, curr_offset,
                                          num_nodes, sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        node_peer = shard.begin();
        advance(node_peer, nodes_send_lo);

        for (unsigned int j = 0; j < num_nodes; j++)
        {
            node_peer->second.Serialize(sharding_structure, curr_offset);
            curr_offset += IP_SIZE + PORT_SIZE;

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      node_peer->second);

            node_peer++;
        }
    }

    // For this version, DS leader is part of the X nodes to receive and share Tx bodies
    if (true)
    {
        m_sharingAssignment.clear();

        for (unsigned int i = num_ds_nodes; i < m_mediator.m_DSCommittee.size();
             i++)
        {
            m_sharingAssignment.emplace_back(
                m_mediator.m_DSCommittee.at(i).second);
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
    SerializeShardingStructure(sharding_structure);

    unsigned int txn_sharing_offset = sharding_structure.size();
    AppendSharingSetupToShardingStructure(sharding_structure,
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

    LOG_STATE("[SHCON][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "] BGIN");

    cl->StartConsensus(sharding_structure, sharding_structure.size());

    return true;
}

void DirectoryService::SaveTxnBodySharingAssignment(
    const vector<unsigned char>& sharding_structure, unsigned int curr_offset)
{
    // Transaction body sharing setup
    // Everyone (DS and non-DS) needs to remember their sharing assignments for this particular block

    // Transaction body sharing assignments:
    // PART 1. Select X random nodes from DS committee for receiving Tx bodies and broadcasting to other DS nodes
    // PART 2. Select X random nodes per shard for receiving Tx bodies and broadcasting to other nodes in the shard
    // PART 3. Select X random nodes per shard for sending Tx bodies to the receiving nodes in other committees (DS and shards)

    // Message format:
    // [4-byte num of DS nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committees]
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // ...

    // To-do: Put in the logic here for checking the sharing configuration

    uint32_t num_ds_nodes = Serializable::GetNumber<uint32_t>(
        sharding_structure, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forwarders inside the DS committee (" << num_ds_nodes << "):");

    vector<Peer> ds_receivers;

    bool i_am_forwarder = false;
    for (uint32_t i = 0; i < num_ds_nodes; i++)
    {
        // TODO: Handle exceptions
        ds_receivers.emplace_back(sharding_structure, curr_offset);
        curr_offset += IP_SIZE + PORT_SIZE;

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "  IP: " << ds_receivers.back().GetPrintableIPAddress()
                           << " Port: "
                           << ds_receivers.back().m_listenPortHost);

        if (ds_receivers.back() == m_mediator.m_selfPeer)
        {
            i_am_forwarder = true;
        }
    }

    m_sharingAssignment.clear();

    if ((i_am_forwarder == true)
        && (m_mediator.m_DSCommittee.size() > num_ds_nodes))
    {
        for (unsigned int i = 0; i < m_mediator.m_DSCommittee.size(); i++)
        {
            bool is_a_receiver = false;

            if (num_ds_nodes > 0)
            {
                for (unsigned int j = 0; j < ds_receivers.size(); j++)
                {
                    if (m_mediator.m_DSCommittee.at(i).second
                        == ds_receivers.at(j))
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
    std::vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // To-do: Put in the logic here for checking the proposed sharding structure
    // We have some below but might not be enough

    m_shards.clear();
    m_publicKeyToShardIdMap.clear();

    // Sharding structure message format:

    // [4-byte num of committees]
    // [4-byte committee size]
    //   [33-byte public key]
    //   [33-byte public key]
    //   ...
    // [4-byte committee size]
    //   [33-byte public key]
    //   [33-byte public key]
    //   ...
    // ...
    lock_guard<mutex> g(m_mutexAllPoWConns);

    unsigned int curr_offset = 0;
    // unsigned int viewChangecounter = Serializable::GetNumber<uint32_t>(sharding_structure, curr_offset, sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    // 4-byte num of committees
    uint32_t numOfComms = Serializable::GetNumber<uint32_t>(
        sharding_structure, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of committees = " << numOfComms);

    for (unsigned int i = 0; i < numOfComms; i++)
    {
        m_shards.emplace_back();

        // 4-byte committee size
        uint32_t shard_size = Serializable::GetNumber<uint32_t>(
            sharding_structure, curr_offset, sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Committee size = " << shard_size);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Members:");

        for (unsigned int j = 0; j < shard_size; j++)
        {
            PubKey memberPubkey(sharding_structure, curr_offset);
            curr_offset += PUB_KEY_SIZE;

            auto memberPeer = m_allPoWConns.find(memberPubkey);
            if (memberPeer == m_allPoWConns.end())
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Shard node not inside m_allPoWConns. "
                              << memberPeer->second.GetPrintableIPAddress()
                              << " Port: "
                              << memberPeer->second.m_listenPortHost);

                m_hasAllPoWconns = false;
                std::unique_lock<std::mutex> lk(m_MutexCVAllPowConn);

                RequestAllPoWConn();
                while (!m_hasAllPoWconns)
                {
                    cv_allPowConns.wait(lk);
                }
                memberPeer = m_allPoWConns.find(memberPubkey);

                if (memberPeer == m_allPoWConns.end())
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "Sharding validator error");
                    // throw exception();
                    return false;
                }
            }

            // To-do: Should we check for a public key that's been assigned to more than 1 shard?
            m_shards.back().emplace(memberPubkey, memberPeer->second);
            m_publicKeyToShardIdMap.emplace(memberPubkey, i);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      " PubKey = "
                          << DataConversion::SerializableToHexStr(memberPubkey)
                          << " at "
                          << memberPeer->second.GetPrintableIPAddress()
                          << " Port: " << memberPeer->second.m_listenPortHost);
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
