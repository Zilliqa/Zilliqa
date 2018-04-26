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
        LOG_GENERAL(INFO,
                    "Zero Pow2 collected, numOfComms is temporarlly set to 1");
        numOfComms = 1;
    }

    for (unsigned int i = 0; i < numOfComms; i++)
    {
        m_shards.push_back(map<PubKey, Peer>());
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
        m_sortedPoW2s.insert(make_pair(sortHash, key));
    }

    lock_guard<mutex> g(m_mutexAllPoWConns, adopt_lock);
    unsigned int i = 0;
    for (auto& kv : m_sortedPoW2s)
    {
        PubKey key = kv.second;
        map<PubKey, Peer>& shard = m_shards.at(i % numOfComms);
        shard.insert(make_pair(key, m_allPoWConns.at(key)));
        m_publicKeyToShardIdMap.insert(make_pair(key, i % numOfComms));
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
                  "Committee size = " << shard.size());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Members:");

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

    // kill first ds leader
    // if (m_consensusMyID == 0 && temp_todie)
    //{
    //    LOG_GENERAL(INFO, "I am killing myself to test view change");
    //    throw exception();
    // }

    // Create new consensus object

    // Dummy values for now
    uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    m_consensusObject.reset(new ConsensusLeader(
        consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommitteePubKeys,
        m_mediator.m_DSCommitteeNetworkInfo,
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

#ifdef STAT_TEST
    LOG_STATE("[SHCON][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "] BGIN");
#endif // STAT_TEST

    cl->StartConsensus(sharding_structure, sharding_structure.size());

    return true;
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
        m_shards.push_back(map<PubKey, Peer>());

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
            m_shards.back().insert(make_pair(memberPubkey, memberPeer->second));
            m_publicKeyToShardIdMap.insert(make_pair(memberPubkey, i));

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      " PubKey = "
                          << DataConversion::SerializableToHexStr(memberPubkey)
                          << " at "
                          << memberPeer->second.GetPrintableIPAddress()
                          << " Port: " << memberPeer->second.m_listenPortHost);
        }
    }

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
        m_mediator.m_selfKey.first, m_mediator.m_DSCommitteePubKeys,
        m_mediator.m_DSCommitteeNetworkInfo,
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
    // unique_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);

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
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "To-do: Code up the logic for if we didn't get any "
                  "submissions at all");
        // throw exception();
        return;
    }

    if (m_mode == PRIMARY_DS)
    {
        if (!RunConsensusOnShardingWhenDSPrimary())
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Exception encountered with running sharding on ds leader");
            // throw exception();
            return;
        }
    }
    else
    {
        if (!RunConsensusOnShardingWhenDSBackup())
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Exception encountered with running sharding on ds backup")
            // throw exception();
            return;
        }
    }

    SetState(SHARDING_CONSENSUS);

    if (m_mode != PRIMARY_DS)
    {
        std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeSharding);
        if (cv_viewChangeSharding.wait_for(
                cv_lk, std::chrono::seconds(VIEWCHANGE_TIME))
            == std::cv_status::timeout)
        {
            //View change.
            //TODO: This is a simplified version and will be review again.
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Initiated sharding structure consensus view change. ");
            InitViewChange();
        }
    }
}
#endif // IS_LOOKUP_NODE
