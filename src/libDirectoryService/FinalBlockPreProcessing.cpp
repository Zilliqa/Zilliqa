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
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void DirectoryService::ExtractDataFromMicroblocks(
    TxnHash& microblockTrieRoot, std::vector<BlockHash>& microBlockTxHashes,
    uint256_t& allGasLimit, uint256_t& allGasUsed, uint32_t& numTxs,
    std::vector<bool>& isMicroBlockEmpty, uint32_t& numMicroBlocks) const
{
    LOG_MARKER();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    auto blockNum
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;
    unsigned int i = 1;

    for (auto& microBlock : m_microBlocks)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Micro block " << i << " has "
                                 << microBlock.GetHeader().GetNumTxs()
                                 << " transactions.");

#ifdef STAT_TEST
        LOG_STATE("[STATS][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << i << "    ]["
                             << microBlock.GetHeader().GetNumTxs()
                             << "] PROPOSED");
#endif // STAT_TEST
        i++;

        microBlockTxHashes.emplace_back(microBlock.GetHeader().GetTxRootHash());
        allGasLimit += microBlock.GetHeader().GetGasLimit();
        allGasUsed += microBlock.GetHeader().GetGasUsed();
        numTxs += microBlock.GetHeader().GetNumTxs();

        ++numMicroBlocks;

        bool isEmpty = microBlock.GetHeader().GetNumTxs() == 0;

        if (!isVacuousEpoch && !isEmpty)
        {
            m_mediator.m_node->m_unavailableMicroBlocks[blockNum].insert(
                microBlock.GetHeader().GetTxRootHash());

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Added " << microBlock.GetHeader().GetTxRootHash()
                               << " for unavailable"
                               << " MicroBlock " << blockNum);
        }

        isMicroBlockEmpty.push_back(isEmpty);
    }

    if (m_mediator.m_node->m_unavailableMicroBlocks.find(blockNum)
            != m_mediator.m_node->m_unavailableMicroBlocks.end()
        && m_mediator.m_node->m_unavailableMicroBlocks[blockNum].size() > 0)
    {
        unique_lock<mutex> g(m_mediator.m_node->m_mutexAllMicroBlocksRecvd);
        m_mediator.m_node->m_allMicroBlocksRecvd = false;
    }

    microblockTrieRoot = ComputeTransactionsRoot(microBlockTxHashes);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Proposed FinalBlock hash : " << DataConversion::charArrToHexStr(
                  microblockTrieRoot.asArray()));
}

void DirectoryService::ComposeFinalBlockCore()
{
    LOG_MARKER();

    TxnHash microblockTrieRoot;
    std::vector<BlockHash> microBlockTxHashes;
    uint8_t type = TXBLOCKTYPE::FINAL;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint256_t allGasLimit = 0;
    uint256_t allGasUsed = 0;
    uint32_t numTxs = 0;
    std::vector<bool> isMicroBlockEmpty;
    uint32_t numMicroBlocks = 0;

    ExtractDataFromMicroblocks(microblockTrieRoot, microBlockTxHashes,
                               allGasLimit, allGasUsed, numTxs,
                               isMicroBlockEmpty, numMicroBlocks);

    m_microBlocks.clear();

    BlockHash prevHash;
    uint256_t timestamp = get_time_as_int();

    uint256_t blockNum = 0;
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        TxBlock lastBlock = m_mediator.m_txBlockChain.GetLastBlock();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        lastBlock.GetHeader().Serialize(vec, 0);
        sha2.Update(vec);
        vector<unsigned char> hashVec = sha2.Finalize();
        copy(hashVec.begin(), hashVec.end(), prevHash.asArray().begin());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Prev block hash as per leader "
                      << prevHash.hex() << endl
                      << "TxBlockHeader: " << lastBlock.GetHeader());
        blockNum = lastBlock.GetHeader().GetBlockNum() + 1;
    }

    if (m_mediator.m_dsBlockChain.GetBlockCount() <= 0)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__ << ": "
                                         << __FUNCTION__ << ")");
    }

    DSBlock lastDSBlock = m_mediator.m_dsBlockChain.GetLastBlock();
    uint256_t lastDSBlockNum = lastDSBlock.GetHeader().GetBlockNum();
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    vector<unsigned char> vec;
    lastDSBlock.GetHeader().Serialize(vec, 0);
    sha2.Update(vec);
    vector<unsigned char> hashVec = sha2.Finalize();
    BlockHash dsBlockHeader;
    copy(hashVec.begin(), hashVec.end(), dsBlockHeader.asArray().begin());

    StateHash stateRoot = StateHash();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (isVacuousEpoch)
    {
        AccountStore::GetInstance().UpdateStateTrieAll();
        stateRoot = AccountStore::GetInstance().GetStateRootHash();
    }

    // Make sure signature placeholders are of the expected size (in particular, the bitmaps)
    // This is because backups will save the final block before consensus inside m_finalBlockMessage
    // Then, m_finalBlockMessage will be updated after consensus (for the cosig values)
    m_finalBlock.reset(new TxBlock(
        TxBlockHeader(type, version, allGasLimit, allGasUsed, prevHash,
                      blockNum, timestamp, microblockTrieRoot, stateRoot,
                      numTxs, numMicroBlocks, m_mediator.m_selfKey.second,
                      lastDSBlockNum, dsBlockHeader, m_viewChangeCounter),
        vector<bool>(isMicroBlockEmpty), vector<TxnHash>(microBlockTxHashes),
        CoSignatures(m_mediator.m_DSCommitteePubKeys.size())));

#ifdef STAT_TEST
    LOG_STATE("[STATS][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "][" << m_finalBlock->GetHeader().GetNumTxs()
                         << "] FINAL");
#endif // STAT_TEST

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block proposed with "
                  << m_finalBlock->GetHeader().GetNumTxs() << " transactions.");
}

void DirectoryService::AppendSharingSetupToFinalBlockMessage(
    vector<unsigned char>& finalBlockMessage, unsigned int curr_offset)
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
              "debug " << m_mediator.m_DSCommitteeNetworkInfo.size() << " "
                       << TX_SHARING_CLUSTER_SIZE);

    uint32_t num_ds_nodes
        = (m_mediator.m_DSCommitteeNetworkInfo.size() < TX_SHARING_CLUSTER_SIZE)
        ? m_mediator.m_DSCommitteeNetworkInfo.size()
        : TX_SHARING_CLUSTER_SIZE;
    Serializable::SetNumber<uint32_t>(finalBlockMessage, curr_offset,
                                      num_ds_nodes, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forwarders inside the DS committee (" << num_ds_nodes << "):");

    for (unsigned int i = 0; i < m_consensusMyID; i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.at(i).Serialize(finalBlockMessage,
                                                            curr_offset);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  m_mediator.m_DSCommitteeNetworkInfo.at(i));
        curr_offset += IP_SIZE + PORT_SIZE;
    }

    // when i == m_consensusMyID use m_mediator.m_selfPeer since IP/ port in
    // m_mediator.m_DSCommitteeNetworkInfo.at(m_consensusMyID) is zeroed out
    m_mediator.m_selfPeer.Serialize(finalBlockMessage, curr_offset);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              m_mediator.m_selfPeer);
    curr_offset += IP_SIZE + PORT_SIZE;

    for (unsigned int i = m_consensusMyID + 1; i < num_ds_nodes; i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.at(i).Serialize(finalBlockMessage,
                                                            curr_offset);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  m_mediator.m_DSCommitteeNetworkInfo.at(i));
        curr_offset += IP_SIZE + PORT_SIZE;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of shards: " << m_shards.size());

    Serializable::SetNumber<uint32_t>(finalBlockMessage, curr_offset,
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

        Serializable::SetNumber<uint32_t>(finalBlockMessage, curr_offset,
                                          num_nodes, sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        map<PubKey, Peer>::const_iterator node_peer = shard.begin();
        for (unsigned int j = 0; j < num_nodes; j++)
        {
            node_peer->second.Serialize(finalBlockMessage, curr_offset);
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

        Serializable::SetNumber<uint32_t>(finalBlockMessage, curr_offset,
                                          num_nodes, sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        node_peer = shard.begin();
        advance(node_peer, nodes_send_lo);

        for (unsigned int j = 0; j < num_nodes; j++)
        {
            node_peer->second.Serialize(finalBlockMessage, curr_offset);
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

        for (unsigned int i = num_ds_nodes;
             i < m_mediator.m_DSCommitteeNetworkInfo.size(); i++)
        {
            m_sharingAssignment.push_back(
                m_mediator.m_DSCommitteeNetworkInfo.at(i));
        }
    }
}

vector<unsigned char> DirectoryService::ComposeFinalBlockMessage()
{
    LOG_MARKER();

    vector<unsigned char> finalBlockMessage;
    unsigned int curr_offset = 0;

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    {
        unique_lock<mutex> g(m_mediator.m_node->m_mutexUnavailableMicroBlocks,
                             defer_lock);
        unique_lock<mutex> g2(m_mediator.m_node->m_mutexAllMicroBlocksRecvd,
                              defer_lock);
        lock(g, g2);

        if (isVacuousEpoch && !m_mediator.m_node->m_allMicroBlocksRecvd)
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Waiting for microblocks before composing final block. Count: "
                    << m_mediator.m_node->m_unavailableMicroBlocks.size());
            for (auto it : m_mediator.m_node->m_unavailableMicroBlocks)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for finalblock " << it.first << ". Count "
                                                    << it.second.size());
                for (auto it2 : it.second)
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              it2);
                }
            }

            m_mediator.m_node->m_cvAllMicroBlocksRecvd.wait(
                g, [this] { return m_mediator.m_node->m_allMicroBlocksRecvd; });
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "All microblocks recvd, moving to compose final block");
        }
    }

    ComposeFinalBlockCore(); // stores it in m_finalBlock

    m_finalBlock->Serialize(finalBlockMessage, curr_offset);
    curr_offset += finalBlockMessage.size();

    AppendSharingSetupToFinalBlockMessage(finalBlockMessage, curr_offset);

    // At this point, cosigs are still not updated inside m_finalBlockMessage
    // Update will be done in ProcessFinalBlockConsensusWhenDone
    m_finalBlockMessage = finalBlockMessage;
    return finalBlockMessage;
}

bool DirectoryService::RunConsensusOnFinalBlockWhenDSPrimary()
{
    LOG_MARKER();

    // Compose the final block from all the microblocks
    // I guess only the leader has to do this
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the leader DS node. Creating final block.");

    // finalBlockMessage = serialized final block + tx-body sharing setup
    vector<unsigned char> finalBlockMessage = ComposeFinalBlockMessage();

    // kill first ds leader
    //if (m_consensusMyID == 0 && temp_todie)
    //{
    //    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //                 "I am killing myself to test view change");
    //    throw exception();
    //}

    // Create new consensus object
    // Dummy values for now
    //uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommitteePubKeys,
        m_mediator.m_DSCommitteeNetworkInfo,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(FINALBLOCKCONSENSUS),
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
#ifdef STAT_TEST
    if (m_mode == PRIMARY_DS)
    {
        LOG_STATE("[FBCON]["
                  << setw(15) << left
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                  << m_mediator.m_txBlockChain.GetBlockCount() << "] BGIN");
    }
#endif // STAT_TEST

    cl->StartConsensus(finalBlockMessage, TxBlockHeader::SIZE);

    return true;
}

// Check type (must be final block type)
bool DirectoryService::CheckBlockTypeIsFinal()
{
    LOG_MARKER();

    if (m_finalBlock->GetHeader().GetType() != TXBLOCKTYPE::FINAL)
    {
        LOG_GENERAL(WARNING,
                    "Type check failed. Expected: "
                        << (unsigned int)TXBLOCKTYPE::FINAL << " Actual: "
                        << (unsigned int)m_finalBlock->GetHeader().GetType());
        return false;
    }

    return true;
}

// Check version (must be most current version)
bool DirectoryService::CheckFinalBlockVersion()
{
    LOG_MARKER();

    if (m_finalBlock->GetHeader().GetVersion() != BLOCKVERSION::VERSION1)
    {
        LOG_GENERAL(
            WARNING,
            "Version check failed. Expected: "
                << (unsigned int)BLOCKVERSION::VERSION1 << " Actual: "
                << (unsigned int)m_finalBlock->GetHeader().GetVersion());
        return false;
    }

    return true;
}

// Check block number (must be = 1 + block number of last Tx block header in the Tx blockchain)
bool DirectoryService::CheckFinalBlockNumber()
{
    LOG_MARKER();

    const uint256_t& finalblockBlocknum
        = m_finalBlock->GetHeader().GetBlockNum();
    uint256_t expectedBlocknum = 0;
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        expectedBlocknum
            = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1;
    }
    if (finalblockBlocknum != expectedBlocknum)
    {
        LOG_GENERAL(WARNING,
                    "Block number check failed. Expected: "
                        << expectedBlocknum
                        << " Actual: " << finalblockBlocknum);
        return false;
    }
    else
    {
        LOG_GENERAL(
            INFO,
            "finalblockBlocknum = expectedBlocknum = " << expectedBlocknum);
    }

    return true;
}

// Check previous hash (must be = sha2-256 digest of last Tx block header in the Tx blockchain)
bool DirectoryService::CheckPreviousFinalBlockHash()
{
    LOG_MARKER();

    const BlockHash& finalblockPrevHash
        = m_finalBlock->GetHeader().GetPrevHash();
    BlockHash expectedPrevHash;

    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().Serialize(vec, 0);
        sha2.Update(vec);
        vector<unsigned char> hashVec = sha2.Finalize();
        copy(hashVec.begin(), hashVec.end(),
             expectedPrevHash.asArray().begin());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "m_mediator.m_txBlockChain.GetLastBlock().GetHeader():"
                      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader());
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Prev block hash recvd: "
                  << finalblockPrevHash.hex() << endl
                  << "Prev block hash expected: " << expectedPrevHash.hex()
                  << endl
                  << "TxBlockHeader: "
                  << m_mediator.m_txBlockChain.GetLastBlock().GetHeader());

    if (finalblockPrevHash != expectedPrevHash)
    {
        LOG_GENERAL(WARNING, "Previous hash check failed.");
        return false;
    }

    return true;
}

// Check timestamp (must be greater than timestamp of last Tx block header in the Tx blockchain)
bool DirectoryService::CheckFinalBlockTimestamp()
{
    LOG_MARKER();

    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        const TxBlock& lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
        uint256_t finalblockTimestamp
            = m_finalBlock->GetHeader().GetTimestamp();
        uint256_t lastTxBlockTimestamp = lastTxBlock.GetHeader().GetTimestamp();
        if (finalblockTimestamp <= lastTxBlockTimestamp)
        {
            LOG_GENERAL(WARNING,
                        "Timestamp check failed. Last Tx Block: "
                            << lastTxBlockTimestamp
                            << " Final block: " << finalblockTimestamp);
            return false;
        }
    }

    return true;
}

// Check microblock hashes
bool DirectoryService::CheckMicroBlockHashes()
{
    LOG_MARKER();

    auto& txRootHashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();

    // O(n^2) might be fine since number of shards is low
    // If its slow on benchmarking, may be first populate an unordered_set and then std::find
    for (auto& microBlockTxHash : txRootHashesInMicroBlocks)
    {
        bool found = false;
        for (auto& microBlock : m_microBlocks)
        {
            if (microBlock.GetHeader().GetTxRootHash() == microBlockTxHash)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

// Check microblock hashes root
bool DirectoryService::CheckMicroBlockHashRoot()
{
    LOG_MARKER();

    auto& txRootHashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();

    TxnHash microBlocksHash
        = ComputeTransactionsRoot(txRootHashesInMicroBlocks);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Expected FinalBlock hash : " << DataConversion::charArrToHexStr(
                  microBlocksHash.asArray()));

    if (m_finalBlock->GetHeader().GetTxRootHash() != microBlocksHash)
    {
        LOG_GENERAL(WARNING,
                    "Microblock root hash in proposed final block by "
                    "leader is incorrect");
        return false;
    }

    return true;
}

bool DirectoryService::CheckIsMicroBlockEmpty()
{
    LOG_MARKER();

    auto& txRootHashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();

    for (unsigned int i = 0; i < txRootHashesInMicroBlocks.size(); i++)
    {
        LOG_GENERAL(INFO,
                    "Microblock"
                        << i << ";"
                        << "Roothash:" << txRootHashesInMicroBlocks[i]
                        << ";IsMicroBlockEmpty:"
                        << m_finalBlock->GetIsMicroBlockEmpty().size());
        for (auto& microBlock : m_microBlocks)
        {
            LOG_GENERAL(INFO,
                        "Checking " << microBlock.GetHeader().GetTxRootHash());
            if (microBlock.GetHeader().GetTxRootHash()
                == txRootHashesInMicroBlocks[i])
            {
                if (m_finalBlock->GetIsMicroBlockEmpty()[i]
                    != (microBlock.GetHeader().GetNumTxs() == 0))
                {
                    LOG_GENERAL(WARNING,
                                "IsMicroBlockEmpty in proposed final "
                                "block is incorrect "
                                    << i << " Expected: "
                                    << (microBlock.GetHeader().GetNumTxs() == 0)
                                    << " Received: "
                                    << m_finalBlock->GetIsMicroBlockEmpty()[i]);
                    return false;
                }
                break;
            }
        }
    }

    return true;
}

// Check state root
bool DirectoryService::CheckStateRoot()
{
    LOG_MARKER();

    StateHash stateRoot = StateHash();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (isVacuousEpoch)
    {
        AccountStore::GetInstance().PrintAccountState();
        stateRoot = AccountStore::GetInstance().GetStateRootHash();
    }

    if (stateRoot != m_finalBlock->GetHeader().GetStateRootHash())
    {
        LOG_GENERAL(WARNING,
                    "State root doesn't match. Expected = "
                        << stateRoot << ". "
                        << "Received = "
                        << m_finalBlock->GetHeader().GetStateRootHash());
        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "State root matched "
                  << m_finalBlock->GetHeader().GetStateRootHash());

    return true;
}

bool DirectoryService::CheckFinalBlockValidity()
{
    LOG_MARKER();

    if (!CheckBlockTypeIsFinal() || !CheckFinalBlockVersion()
        || !CheckFinalBlockNumber() || !CheckPreviousFinalBlockHash()
        || !CheckFinalBlockTimestamp() || !CheckMicroBlockHashes()
        || !CheckMicroBlockHashRoot() || !CheckIsMicroBlockEmpty()
        || !CheckStateRoot())
    {
        return false;
    }

    // TODO: Check gas limit (must satisfy some equations)
    // TODO: Check gas used (must be <= gas limit)
    // TODO: Check pubkey (must be valid and = shard leader)
    // TODO: Check parent DS hash (must be = digest of last DS block header in the DS blockchain)
    // TODO: Check parent DS block number (must be = block number of last DS block header in the DS blockchain)

    return true;
}

void DirectoryService::SaveTxnBodySharingAssignment(
    const vector<unsigned char>& finalblock, unsigned int& curr_offset)
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
        finalblock, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forwarders inside the DS committee (" << num_ds_nodes << "):");

    vector<Peer> ds_receivers;

    bool i_am_forwarder = false;
    for (uint32_t i = 0; i < num_ds_nodes; i++)
    {
        // TODO: Handle exceptions
        ds_receivers.push_back(Peer(finalblock, curr_offset));
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
        && (m_mediator.m_DSCommitteeNetworkInfo.size() > num_ds_nodes))
    {
        for (unsigned int i = 0; i < m_mediator.m_DSCommitteeNetworkInfo.size();
             i++)
        {
            bool is_a_receiver = false;

            if (num_ds_nodes > 0)
            {
                for (unsigned int j = 0; j < ds_receivers.size(); j++)
                {
                    if (m_mediator.m_DSCommitteeNetworkInfo.at(i)
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
                m_sharingAssignment.push_back(
                    m_mediator.m_DSCommitteeNetworkInfo.at(i));
            }
        }
    }
}

bool DirectoryService::WaitForTxnBodies()
{
    LOG_MARKER();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    {
        unique_lock<mutex> g(m_mediator.m_node->m_mutexUnavailableMicroBlocks,
                             defer_lock);
        unique_lock<mutex> g2(m_mediator.m_node->m_mutexAllMicroBlocksRecvd,
                              defer_lock);
        lock(g, g2);

        if (isVacuousEpoch && !m_mediator.m_node->m_allMicroBlocksRecvd)
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Waiting for microblocks before verifying final block. Count: "
                    << m_mediator.m_node->m_unavailableMicroBlocks.size());
            for (auto it : m_mediator.m_node->m_unavailableMicroBlocks)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for finalblock " << it.first << ". Count "
                                                    << it.second.size());
                for (auto it2 : it.second)
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              it2);
                }
            }

            m_mediator.m_node->m_cvAllMicroBlocksRecvd.wait(
                g, [this] { return m_mediator.m_node->m_allMicroBlocksRecvd; });
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "All microblocks recvd, moving to verify final block");
        }
    }

    return true;
}

void DirectoryService::LoadUnavailableMicroBlocks()
{
    LOG_MARKER();

    auto blockNum = m_finalBlock->GetHeader().GetBlockNum();
    auto& txRootHashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();
    lock_guard<mutex> g(m_mediator.m_node->m_mutexUnavailableMicroBlocks);
    for (auto& microBlockTxHash : txRootHashesInMicroBlocks)
    {
        for (auto& microBlock : m_microBlocks)
        {
            if (microBlock.GetHeader().GetTxRootHash() == microBlockTxHash
                && microBlock.GetHeader().GetNumTxs() > 0)
            {
                m_mediator.m_node->m_unavailableMicroBlocks[blockNum].insert(
                    microBlockTxHash);
                break;
            }
        }
    }

    if (m_mediator.m_node->m_unavailableMicroBlocks.find(blockNum)
            != m_mediator.m_node->m_unavailableMicroBlocks.end()
        && m_mediator.m_node->m_unavailableMicroBlocks[blockNum].size() > 0)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "setting false for unavailable microblock " << m_consensusID);
        unique_lock<mutex> g(m_mediator.m_node->m_mutexAllMicroBlocksRecvd);
        m_mediator.m_node->m_allMicroBlocksRecvd = false;
    }
}

bool DirectoryService::FinalBlockValidator(
    const vector<unsigned char>& finalblock, vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    unsigned int curr_offset = 0;

    m_finalBlock.reset(new TxBlock(finalblock, curr_offset));
    curr_offset += m_finalBlock->GetSerializedSize();

    WaitForTxnBodies();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    if (isVacuousEpoch)
    {
        AccountStore::GetInstance().UpdateStateTrieAll();
    }

    if (!CheckFinalBlockValidity())
    {
        LOG_GENERAL(WARNING,
                    "To-do: What to do if proposed microblock is not valid?");
        // throw exception();
        // TODO: microblock is invalid
        return false;
    }

    if (!isVacuousEpoch)
    {
        LoadUnavailableMicroBlocks();
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block "
                  << m_finalBlock->GetHeader().GetBlockNum()
                  << " received with prevhash 0x"
                  << DataConversion::charArrToHexStr(
                         m_finalBlock->GetHeader().GetPrevHash().asArray()));

    m_microBlocks.clear();

    SaveTxnBodySharingAssignment(finalblock, curr_offset);

    m_finalBlockMessage = finalblock;

    return true;
}

bool DirectoryService::RunConsensusOnFinalBlockWhenDSBackup()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a backup DS node. Waiting for final block announcement.");

    // Create new consensus object

    // Dummy values for now
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return FinalBlockValidator(message, errorMsg);
    };

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first,
        m_mediator.m_DSCommitteePubKeys, m_mediator.m_DSCommitteeNetworkInfo,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(FINALBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

void DirectoryService::RunConsensusOnFinalBlock()
{
    LOG_MARKER();

    SetState(FINALBLOCK_CONSENSUS_PREP);

    if (m_mode == PRIMARY_DS)
    {
        /*
        bool isVacuousEpoch
            = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

        if (isVacuousEpoch)
        {
            LOG_GENERAL(
                "I am going to sleep for 5 seconds for vacuous epoch.");
            this_thread::sleep_for(chrono::seconds(5));
        }
        */
        if (!RunConsensusOnFinalBlockWhenDSPrimary())
        {
            LOG_GENERAL(WARNING,
                        "Throwing exception after "
                        "RunConsensusOnFinalBlockWhenDSPrimary");
            // throw exception();
            return;
        }
    }
    else
    {
        if (!RunConsensusOnFinalBlockWhenDSBackup())
        {
            LOG_GENERAL(WARNING,
                        "Throwing exception after "
                        "RunConsensusOnFinalBlockWhenDSBackup");
            // throw exception();
            return;
        }
    }

    SetState(FINALBLOCK_CONSENSUS);
    cv_finalBlockConsensusObject.notify_all();

    if (m_mode != PRIMARY_DS)
    {
        std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeFinalBlock);
        if (cv_viewChangeFinalBlock.wait_for(
                cv_lk, std::chrono::seconds(VIEWCHANGE_TIME))
            == std::cv_status::timeout)
        {
            //View change.
            //TODO: This is a simplified version and will be review again.
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Initiated final block view change. ");
            InitViewChange();
        }
    }
}
#endif // IS_LOOKUP_NODE
