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
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

void DirectoryService::ComposeDSBlock(
    const vector<pair<array<unsigned char, 32>, PubKey>>& sortedPoWSolns)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ComposeDSBlock not expected to be "
                    "called from LookUp node.");
        return;
    }

    LOG_MARKER();

    // Compute hash of previous DS block header
    BlockHash prevHash;
    if (m_mediator.m_dsBlockChain.GetBlockCount() > 0)
    {
        DSBlock lastBlock = m_mediator.m_dsBlockChain.GetLastBlock();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        const DSBlockHeader& lastHeader = lastBlock.GetHeader();
        lastHeader.Serialize(vec, 0);
        sha2.Update(vec);
        const vector<unsigned char>& resVec = sha2.Finalize();
        copy(resVec.begin(), resVec.end(), prevHash.asArray().begin());
    }

    // Assemble DS block header
    const array<unsigned char, 32> winnerPoW = sortedPoWSolns.front().first;
    const PubKey& winnerKey = sortedPoWSolns.front().second;

    if (!POW::GetInstance().CheckSolnAgainstsTargetedDifficulty(
            DataConversion::charArrToHexStr(winnerPoW), DS_POW_DIFFICULTY))
    {
        LOG_GENERAL(WARNING, "No soln met the DS difficulty level");
        //TODO: To handle if no PoW soln can meet DS difficulty level.
    }

    uint64_t blockNum = 0;
    uint8_t dsDifficulty = DS_POW_DIFFICULTY;
    uint8_t difficulty = POW_DIFFICULTY;
    if (m_mediator.m_dsBlockChain.GetBlockCount() > 0)
    {
        DSBlock lastBlock = m_mediator.m_dsBlockChain.GetLastBlock();
        blockNum = lastBlock.GetHeader().GetBlockNum() + 1;
    }

    // Start to adjust difficulty from second DS block.
    if (blockNum > 1)
    {
        dsDifficulty
            = CalculateNewDSDifficulty(m_mediator.m_dsBlockChain.GetLastBlock()
                                           .GetHeader()
                                           .GetDSDifficulty());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Current DS difficulty "
                      << std::to_string(m_mediator.m_dsBlockChain.GetLastBlock()
                                            .GetHeader()
                                            .GetDSDifficulty())
                      << ", new DS difficulty "
                      << std::to_string(dsDifficulty));

        difficulty
            = CalculateNewDifficulty(m_mediator.m_dsBlockChain.GetLastBlock()
                                         .GetHeader()
                                         .GetDifficulty());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Current difficulty "
                      << std::to_string(m_mediator.m_dsBlockChain.GetLastBlock()
                                            .GetHeader()
                                            .GetDifficulty())
                      << ", new difficulty " << std::to_string(difficulty));
    }

    // Assemble DS block
    // To-do: Handle exceptions.
    // TODO: Revise DS block structure
    m_pendingDSBlock.reset(
        new DSBlock(DSBlockHeader(dsDifficulty, difficulty, prevHash, 0,
                                  winnerKey, m_mediator.m_selfKey.second,
                                  blockNum, get_time_as_int(), SWInfo()),
                    CoSignatures(m_mediator.m_DSCommittee->size())));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New DSBlock created with winning PoW = 0x"
                  << DataConversion::charArrToHexStr(winnerPoW));
}

void DirectoryService::ComputeSharding(
    const vector<pair<array<unsigned char, 32>, PubKey>>& sortedPoWSolns)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ComputeSharding not expected to be "
                    "called from LookUp node.");
        return;
    }

    LOG_MARKER();

    m_shards.clear();
    m_publicKeyToShardIdMap.clear();

    if (m_allPoWs.size() < COMM_SIZE)
    {
        LOG_GENERAL(WARNING, "PoWs recvd less than one shard size");
    }

    std::set<PubKey> setTopPriorityNodes;
    if (m_allPoWs.size() > MAX_SHARD_NODE_NUM)
    {
        LOG_GENERAL(INFO,
                    "PoWs recvd " << m_allPoWs.size()
                                  << " more than max node number "
                                  << MAX_SHARD_NODE_NUM);
        setTopPriorityNodes = FindTopPriorityNodes();
    }

    auto numShardNodes = sortedPoWSolns.size() > MAX_SHARD_NODE_NUM
        ? MAX_SHARD_NODE_NUM
        : sortedPoWSolns.size();

    uint32_t numOfComms = numShardNodes / COMM_SIZE;
    uint32_t max_shard = numOfComms - 1;

    if (numOfComms == 0)
    {
        LOG_GENERAL(WARNING,
                    "Cannot form even one committee "
                        << " number of Pows " << sortedPoWSolns.size()
                        << " Setting numOfcomms to be 1");
        numOfComms = 1;
        max_shard = 0;
    }

    for (unsigned int i = 0; i < numOfComms; i++)
    {
        m_shards.emplace_back();
    }
    map<array<unsigned char, BLOCK_HASH_SIZE>, PubKey> sortedPoWs;
    vector<unsigned char> lastBlockHash(BLOCK_HASH_SIZE);

    if (m_mediator.m_currentEpochNum > 1)
    {
        lastBlockHash = HashUtils::SerializableToHash(
            m_mediator.m_txBlockChain.GetLastBlock());
    }
    for (const auto& kv : sortedPoWSolns)
    {
        const PubKey& key = kv.second;
        if (!setTopPriorityNodes.empty()
            && setTopPriorityNodes.find(key) == setTopPriorityNodes.end())
        {
            LOG_GENERAL(INFO,
                        "Node "
                            << key
                            << " failed to join because priority not enough.");
            continue;
        }
        const array<unsigned char, BLOCK_HASH_SIZE>& powHash = kv.first;

        // sort all PoW submissions according to H(last_block_hash, pow_hash)
        vector<unsigned char> hashVec;
        hashVec.resize(BLOCK_HASH_SIZE + POW_SIZE);
        copy(lastBlockHash.begin(), lastBlockHash.end(), hashVec.begin());
        copy(powHash.begin(), powHash.end(), hashVec.begin() + BLOCK_HASH_SIZE);

        const vector<unsigned char>& sortHashVec
            = HashUtils::BytesToHash(hashVec);
        array<unsigned char, BLOCK_HASH_SIZE> sortHash;
        copy(sortHashVec.begin(), sortHashVec.end(), sortHash.begin());
        sortedPoWs.emplace(sortHash, key);
    }

    unsigned int i = 0;

    for (const auto& kv : sortedPoWs)
    {
        LOG_GENERAL(INFO,
                    "[DSSORT] " << kv.second << " "
                                << DataConversion::charArrToHexStr(kv.first)
                                << endl);
        const PubKey& key = kv.second;
        auto& shard = m_shards.at(min(i / COMM_SIZE, max_shard));
        shard.emplace_back(key, m_allPoWConns.at(key),
                           m_mapNodeReputation[key]);
        m_publicKeyToShardIdMap.emplace(key, min(i / COMM_SIZE, max_shard));
        i++;
    }
}

bool DirectoryService::VerifyPoWOrdering(const DequeOfShard& shards)
{
    //Requires mutex for m_shards
    vector<unsigned char> lastBlockHash(BLOCK_HASH_SIZE, 0);
    set<PubKey> keyset;

    if (m_mediator.m_currentEpochNum > 1)
    {
        lastBlockHash = HashUtils::SerializableToHash(
            m_mediator.m_txBlockChain.GetLastBlock());
    }
    //Temporarily add the old ds to check ordering
    m_allPoWs[m_mediator.m_DSCommittee->back().first]
        = array<unsigned char, BLOCK_HASH_SIZE>();

    vector<unsigned char> hashVec;
    bool ret = true;
    vector<unsigned char> vec(BLOCK_HASH_SIZE);
    for (const auto& shard : shards)
    {
        for (const auto& shardNode : shard)
        {
            const PubKey& toFind = std::get<SHARD_NODE_PUBKEY>(shardNode);
            auto it = m_allPoWs.find(toFind);

            if (it == m_allPoWs.end())
            {
                LOG_GENERAL(WARNING,
                            "Failed to find key in the PoW ordering "
                                << toFind << " " << m_allPoWs.size());
                ret = false;
                break;
            }
            hashVec.clear();
            hashVec.resize(BLOCK_HASH_SIZE + BLOCK_HASH_SIZE);
            copy(lastBlockHash.begin(), lastBlockHash.end(), hashVec.begin());
            copy(it->second.begin(), it->second.end(),
                 hashVec.begin() + BLOCK_HASH_SIZE);
            const vector<unsigned char>& sortHashVec
                = HashUtils::BytesToHash(hashVec);
            LOG_GENERAL(INFO,
                        "[DSSORT]"
                            << DataConversion::Uint8VecToHexStr(sortHashVec)
                            << " " << std::get<SHARD_NODE_PUBKEY>(shardNode));
            if (sortHashVec < vec)
            {
                LOG_GENERAL(
                    WARNING,
                    "Failed to Verify due to bad PoW ordering "
                        << DataConversion::Uint8VecToHexStr(vec) << " "
                        << DataConversion::Uint8VecToHexStr(sortHashVec));
                ret = false;
                break;
            }
            auto r = keyset.insert(std::get<SHARD_NODE_PUBKEY>(shardNode));
            if (!r.second)
            {
                LOG_GENERAL(WARNING,
                            "The key is not unique in the sharding structure "
                                << std::get<SHARD_NODE_PUBKEY>(shardNode));
                ret = false;
                break;
            }
            vec = sortHashVec;
        }
        if (!ret)
        {
            break;
        }
    }
    m_allPoWs.erase(m_mediator.m_DSCommittee->back().first);
    return ret;
}

bool DirectoryService::VerifyNodePriority(const DequeOfShard& shards)
{
    // If the PoW submissions less than the max number of nodes, then all nodes can join, no need to verify.
    if (m_allPoWs.size() <= MAX_SHARD_NODE_NUM)
    {
        return true;
    }

    uint32_t numOutOfMyPriorityList = 0;
    auto setTopPriorityNodes = FindTopPriorityNodes();
    for (const auto& shard : shards)
    {
        for (const auto& shardNode : shard)
        {
            const PubKey& toFind = std::get<SHARD_NODE_PUBKEY>(shardNode);
            if (setTopPriorityNodes.find(toFind) == setTopPriorityNodes.end())
            {
                ++numOutOfMyPriorityList;
                LOG_GENERAL(WARNING,
                            "Node " << toFind
                                    << " is not in my top priority list");
            }
        }
    }

    constexpr float tolerance = 0.02f;
    const uint32_t MAX_NODE_OUT_OF_LIST
        = std::ceil(MAX_SHARD_NODE_NUM * tolerance);
    if (numOutOfMyPriorityList > MAX_NODE_OUT_OF_LIST)
    {
        LOG_GENERAL(WARNING,
                    "Number of node not in my priority "
                        << numOutOfMyPriorityList << " exceed tolerance "
                        << MAX_NODE_OUT_OF_LIST);
        return false;
    }
    return true;
}

void DirectoryService::ComputeTxnSharingAssignments(const Peer& winnerpeer)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ComputeTxnSharingAssignments not "
                    "expected to be called from LookUp node.");
        return;
    }

    LOG_MARKER();

    // PART 1
    // First version: We just take the first X nodes in DS committee
    // Take note that this is the OLD DS committee -> we must consider that winnerpeer is the new DS leader (and the last node in the committee will no longer be a DS node)

    m_DSReceivers.clear();

    uint32_t num_ds_nodes
        = (m_mediator.m_DSCommittee->size() < TX_SHARING_CLUSTER_SIZE)
        ? m_mediator.m_DSCommittee->size()
        : TX_SHARING_CLUSTER_SIZE;

    // Add the new DS leader first
    m_DSReceivers.emplace_back(winnerpeer);
    m_mediator.m_node->m_txnSharingIAmSender = true;
    num_ds_nodes--;

    // Add the rest from the current DS committee
    for (unsigned int i = 0; i < num_ds_nodes; i++)
    {
        if (i != m_consensusMyID)
        {
            m_DSReceivers.emplace_back(m_mediator.m_DSCommittee->at(i).second);
        }
        else
        {
            // when i == m_consensusMyID use m_mediator.m_selfPeer since IP/ port in m_mediator.m_DSCommittee->at(m_consensusMyID).second is zeroed out
            m_DSReceivers.emplace_back(m_mediator.m_selfPeer);
        }
    }

    // PART 2 and 3
    // First version: We just take the first X nodes for receiving and next X nodes for sending

    m_shardReceivers.clear();
    m_shardSenders.clear();

    for (const auto& shard : m_shards)
    {
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
            m_shardReceivers.back().emplace_back(
                std::get<SHARD_NODE_PEER>(*node_peer));
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
            m_shardSenders.back().emplace_back(
                std::get<SHARD_NODE_PEER>(*node_peer));
            node_peer++;
        }
    }
}

bool DirectoryService::RunConsensusOnDSBlockWhenDSPrimary()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnDSBlockWhenDSPrimary not "
                    "expected to be called from LookUp node.");
        return true;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the leader DS node. Creating DS block.");

    lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
    lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
    lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

    // Use a map to sort the soln according to difficulty level
    map<array<unsigned char, 32>, PubKey> PoWOrderSorter;
    for (const auto& powsoln : m_allPoWs)
    {
        PoWOrderSorter[powsoln.second] = powsoln.first;
    }

    // Put it back to vector for easy manipilation and adjustment of the ordering
    vector<pair<array<unsigned char, 32>, PubKey>> sortedPoWSolns;
    for (const auto& kv : PoWOrderSorter)
    {
        sortedPoWSolns.emplace_back(kv);
        LOG_GENERAL(INFO, "0x" << DataConversion::charArrToHexStr(kv.first));
    }

    ComposeDSBlock(sortedPoWSolns);

    // Remove the PoW winner from m_allPoWs so it doesn't get included in sharding structure
    swap(sortedPoWSolns.front(), sortedPoWSolns.back());
    sortedPoWSolns.pop_back();

    // Add the oldest DS committee member to m_allPoWs and m_allPoWConns so it gets included in sharding structure
    sortedPoWSolns.emplace_back(array<unsigned char, 32>(),
                                m_mediator.m_DSCommittee->back().first);

    if (m_mediator.m_DSCommittee->back().first == m_mediator.m_selfKey.second)
    {
        m_allPoWConns.emplace(
            make_pair(m_mediator.m_selfKey.second, m_mediator.m_selfPeer));
    }
    else
    {
        m_allPoWConns.emplace(m_mediator.m_DSCommittee->back());
    }

    const auto& winnerPeer
        = m_allPoWConns.find(m_pendingDSBlock->GetHeader().GetMinerPubKey());

    ClearReputationOfNodeWithoutPoW();
    ComputeSharding(sortedPoWSolns);
    ComputeTxnSharingAssignments(winnerPeer->second);

    // Create new consensus object
    // Dummy values for now
    uint32_t consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    // kill first ds leader (used for view change testing)
    // Either do killing of ds leader or make ds leader do nothing.
    /**
    if (m_consensusMyID == 0 && m_viewChangeCounter < 1)
    {
        LOG_GENERAL(INFO, "I am killing/suspending myself to test view change");
        // throw exception();
        return false;
    }
    **/

    m_consensusObject.reset(new ConsensusLeader(
        consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(DSBLOCKCONSENSUS),
        NodeCommitFailureHandlerFunc(), ShardCommitFailureHandlerFunc()));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "WARNING: Unable to create consensus object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    LOG_STATE(
        "[DSCON]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] BGIN");

    auto announcementGeneratorFunc =
        [this](vector<unsigned char>& dst, unsigned int offset,
               const uint32_t consensusID,
               const vector<unsigned char>& blockHash, const uint16_t leaderID,
               const pair<PrivKey, PubKey>& leaderKey,
               vector<unsigned char>& messageToCosign) mutable -> bool {
        const auto& winnerPeer = m_allPoWConns.find(
            m_pendingDSBlock->GetHeader().GetMinerPubKey());
        return Messenger::SetDSDSBlockAnnouncement(
            dst, offset, consensusID, blockHash, leaderID, leaderKey,
            *m_pendingDSBlock, winnerPeer->second, m_shards, m_DSReceivers,
            m_shardReceivers, m_shardSenders, messageToCosign);
    };

    cl->StartConsensus(announcementGeneratorFunc);

    return true;
}

void DirectoryService::ProcessTxnBodySharingAssignment()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ProcessTxnBodySharingAssignment not "
                    "expected to be called from LookUp node.");
        return;
    }

    bool i_am_forwarder = false;
    for (const auto& receiver : m_DSReceivers)
    {
        if (receiver == m_mediator.m_selfPeer)
        {
            m_mediator.m_node->m_txnSharingIAmSender = true;
            i_am_forwarder = true;
            break;
        }
    }

    unsigned int num_ds_nodes = m_DSReceivers.size();

    m_sharingAssignment.clear();

    if ((i_am_forwarder) && (m_mediator.m_DSCommittee->size() > num_ds_nodes))
    {
        for (const auto& ds : *m_mediator.m_DSCommittee)
        {
            bool is_a_receiver = false;

            if (num_ds_nodes > 0)
            {
                for (const auto& receiver : m_DSReceivers)
                {
                    if (ds.second == receiver)
                    {
                        is_a_receiver = true;
                        break;
                    }
                }
                num_ds_nodes--;
            }

            if (!is_a_receiver)
            {
                m_sharingAssignment.emplace_back(ds.second);
            }
        }
    }
}

bool DirectoryService::DSBlockValidator(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] vector<unsigned char>& errorMsg, const uint32_t consensusID,
    const vector<unsigned char>& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, vector<unsigned char>& messageToCosign)
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::DSBlockValidator not "
                    "expected to be called from LookUp node.");
        return true;
    }

    Peer winnerPeer;

    m_tempDSReceivers.clear();
    m_tempShardReceivers.clear();
    m_tempShardSenders.clear();
    m_tempShards.clear();

    lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
    lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
    lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

    m_pendingDSBlock.reset(new DSBlock);

    if (!Messenger::GetDSDSBlockAnnouncement(
            message, offset, consensusID, blockHash, leaderID, leaderKey,
            *m_pendingDSBlock, winnerPeer, m_tempShards, m_tempDSReceivers,
            m_tempShardReceivers, m_tempShardSenders, messageToCosign))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Messenger::GetDSDSBlockAnnouncement failed.");
        return false;
    }

    // To-do: Put in the logic here for checking the proposed DS block

    auto storedMember
        = m_allPoWConns.find(m_pendingDSBlock->GetHeader().GetMinerPubKey());

    // I know the winner but the winner IP given by the leader is different!
    if (storedMember != m_allPoWConns.end())
    {
        if (storedMember->second != winnerPeer)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "WARNING: Why is the IP of the winner different from "
                      "what I have in m_allPoWConns???");
            return false;
        }
    }
    // I don't know the winner -> store the IP given by the leader
    else
    {
        m_allPoWConns.emplace(m_pendingDSBlock->GetHeader().GetMinerPubKey(),
                              winnerPeer);
    }

    // Start to adjust difficulty from second DS block.
    if (m_pendingDSBlock->GetHeader().GetBlockNum() > 1)
    {
        auto remoteDSDifficulty
            = m_pendingDSBlock->GetHeader().GetDSDifficulty();
        auto localDSDifficulty
            = CalculateNewDSDifficulty(m_mediator.m_dsBlockChain.GetLastBlock()
                                           .GetHeader()
                                           .GetDSDifficulty());
        constexpr uint8_t DIFFICULTY_TOL = 1;
        if (remoteDSDifficulty < localDSDifficulty
            || (remoteDSDifficulty - localDSDifficulty > DIFFICULTY_TOL))
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "WARNING: The ds difficulty "
                          << std::to_string(remoteDSDifficulty)
                          << " from leader not match with local calculated "
                             "result "
                          << std::to_string(localDSDifficulty));
            return false;
        }

        auto remoteDifficulty = m_pendingDSBlock->GetHeader().GetDifficulty();
        auto localDifficulty
            = CalculateNewDifficulty(m_mediator.m_dsBlockChain.GetLastBlock()
                                         .GetHeader()
                                         .GetDifficulty());
        if (remoteDifficulty < localDifficulty
            || (remoteDifficulty - localDifficulty > DIFFICULTY_TOL))
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "WARNING: The difficulty "
                          << std::to_string(remoteDifficulty)
                          << " from leader not match with local calculated "
                             "result "
                          << std::to_string(localDifficulty));
            return false;
        }
    }

    if (!ProcessShardingStructure(m_tempShards, m_tempPublicKeyToShardIdMap,
                                  m_tempMapNodeReputation))
    {
        return false;
    }

    if (!VerifyPoWOrdering(m_tempShards))
    {
        LOG_GENERAL(INFO, "Failed to verify ordering");
        //return false; [TODO] Enable this check after fixing the PoW order issue.
    }

    ClearReputationOfNodeWithoutPoW();
    if (!VerifyNodePriority(m_tempShards))
    {
        LOG_GENERAL(WARNING, "Failed to verify node priority");
        return false;
    }
    //ProcessTxnBodySharingAssignment();

    return true;
}

bool DirectoryService::RunConsensusOnDSBlockWhenDSBackup()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnDSBlockWhenDSBackup not "
                    "expected to be called from LookUp node.");
        return true;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a backup DS node. Waiting for DS block announcement.");

    // Dummy values for now
    uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func
        = [this](const vector<unsigned char>& input, unsigned int offset,
                 vector<unsigned char>& errorMsg, const uint32_t consensusID,
                 const vector<unsigned char>& blockHash,
                 const uint16_t leaderID, const PubKey& leaderKey,
                 vector<unsigned char>& messageToCosign) mutable -> bool {
        return DSBlockValidator(input, offset, errorMsg, consensusID, blockHash,
                                leaderID, leaderKey, messageToCosign);
    };

    m_consensusObject.reset(new ConsensusBackup(
        consensusID, m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
        m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(DSBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

bool DirectoryService::ProcessShardingStructure(
    const DequeOfShard& shards,
    std::map<PubKey, uint32_t>& publicKeyToShardIdMap,
    std::map<PubKey, uint16_t>& mapNodeReputation)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ProcessShardingStructure not "
                    "expected to be called from LookUp node.");
        return true;
    }

    publicKeyToShardIdMap.clear();
    mapNodeReputation.clear();

    for (unsigned int i = 0; i < shards.size(); i++)
    {
        for (const auto& shardNode : shards.at(i))
        {
            const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(shardNode);

            mapNodeReputation[pubKey] = std::get<SHARD_NODE_REP>(shardNode);

            auto storedMember = m_allPoWConns.find(pubKey);

            // I know the member but the member IP given by the leader is different!
            if (storedMember != m_allPoWConns.end())
            {
                if (storedMember->second
                    != std::get<SHARD_NODE_PEER>(shardNode))
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
                m_allPoWConns.emplace(std::get<SHARD_NODE_PUBKEY>(shardNode),
                                      std::get<SHARD_NODE_PEER>(shardNode));
            }

            publicKeyToShardIdMap.emplace(
                std::get<SHARD_NODE_PUBKEY>(shardNode), i);
        }
    }

    return true;
}

void DirectoryService::RunConsensusOnDSBlock(bool isRejoin)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnDSBlock not "
                    "expected to be called from LookUp node.");
        return;
    }

    LOG_GENERAL(INFO,
                "Number of PoW recvd " << m_allPoWs.size() << " "
                                       << m_allDSPoWs.size());

    LOG_MARKER();
    SetState(DSBLOCK_CONSENSUS_PREP);

    {
        lock_guard<mutex> g(m_mutexAllPOW);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Num of PoW sub rec: " << m_allPoWs.size());
        LOG_STATE("[POWR][" << std::setw(15) << std::left
                            << m_mediator.m_selfPeer.GetPrintableIPAddress()
                            << "][" << m_allPoWs.size() << "] ");

        if (m_allPoWs.size() == 0)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "To-do: Code up the logic for if we didn't get any "
                      "submissions at all");
            // throw exception();
            if (!isRejoin)
            {
                return;
            }
        }
    }

    m_mediator.m_node->m_txnSharingIAmSender = false;

    // Upon consensus object creation failure, one should not return from the function, but rather wait for view change.
    bool ConsensusObjCreation = true;
    if (m_mode == PRIMARY_DS)
    {
        ConsensusObjCreation = RunConsensusOnDSBlockWhenDSPrimary();
        if (!ConsensusObjCreation)
        {
            LOG_GENERAL(WARNING,
                        "Error after RunConsensusOnDSBlockWhenDSPrimary");
        }
    }
    else
    {
        ConsensusObjCreation = RunConsensusOnDSBlockWhenDSBackup();
        if (!ConsensusObjCreation)
        {
            LOG_GENERAL(WARNING,
                        "Error after RunConsensusOnDSBlockWhenDSBackup");
        }
    }

    if (ConsensusObjCreation)
    {
        SetState(DSBLOCK_CONSENSUS);
        cv_DSBlockConsensusObject.notify_all();
    }

    // View change will wait for timeout. If conditional variable is notified before timeout, the thread will return
    // without triggering view change.
    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeDSBlock);
    if (cv_viewChangeDSBlock.wait_for(cv_lk,
                                      std::chrono::seconds(VIEWCHANGE_TIME))
        == std::cv_status::timeout)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Initiated DS block view change. ");
        auto func = [this]() -> void { RunConsensusOnViewChange(); };
        DetachedFunction(1, func);
    }
}
