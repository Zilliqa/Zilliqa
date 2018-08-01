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

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <functional>
#include <limits>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libServer/Server.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;

bool Node::ReadAuxilliaryInfoFromFinalBlockMsg(
    const vector<unsigned char>& message, unsigned int& cur_offset,
    uint32_t& shard_id)
{
    // 8-byte block number
    uint64_t dsBlockNum = Serializable::GetNumber<uint64_t>(message, cur_offset,
                                                            sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    // Check block number
    if (!CheckWhetherDSBlockNumIsLatest(dsBlockNum + 1))
    {
        return false;
    }

    // 4-byte consensus id
    uint32_t consensusID = Serializable::GetNumber<uint32_t>(
        message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    if (consensusID != m_consensusID)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus ID is not correct. Expected ID: "
                      << consensusID << " My Consensus ID: " << m_consensusID);
        return false;
    }

    shard_id = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                 sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG shard id is " << (unsigned int)shard_id)

    return true;
}

void Node::StoreState()
{
    LOG_MARKER();
    AccountStore::GetInstance().MoveUpdatesToDisk();
}

void Node::StoreFinalBlock(const TxBlock& txBlock)
{
    AddBlock(txBlock);
    m_mediator.m_currentEpochNum
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;

    // At this point, the transactions in the last Epoch is no longer useful, thus erase.
    EraseCommittedTransactions(m_mediator.m_currentEpochNum - 2);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Storing Tx Block Number: "
                  << txBlock.GetHeader().GetBlockNum()
                  << " with Type: " << txBlock.GetHeader().GetType()
                  << ", Version: " << txBlock.GetHeader().GetVersion()
                  << ", Timestamp: " << txBlock.GetHeader().GetTimestamp()
                  << ", NumTxs: " << txBlock.GetHeader().GetNumTxs());

    // Store Tx Block to disk
    vector<unsigned char> serializedTxBlock;
    txBlock.Serialize(serializedTxBlock, 0);
    BlockStorage::GetBlockStorage().PutTxBlock(
        txBlock.GetHeader().GetBlockNum(), serializedTxBlock);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block " << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                             << " received with prevhash 0x"
                             << DataConversion::charArrToHexStr(
                                    m_mediator.m_txBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetPrevHash()
                                        .asArray()));

    LOG_STATE(
        "[FINBK]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] RECV");
}

bool Node::IsMicroBlockTxRootHashInFinalBlock(
    TxnHash microBlockTxRootHash, StateHash microBlockStateDeltaHash,
    const uint64_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Deleting unavailable "
                  << "microblock txRootHash " << microBlockTxRootHash
                  << " stateDeltaHash " << microBlockStateDeltaHash
                  << " for blocknum " << blocknum);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    auto it = m_unavailableMicroBlocks.find(blocknum);
    bool found
        = (it != m_unavailableMicroBlocks.end()
           && RemoveTxRootHashFromUnavailableMicroBlock(
                  blocknum, microBlockTxRootHash, microBlockStateDeltaHash));
    isEveryMicroBlockAvailable = found && it->second.empty();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Found txRootHash " << microBlockTxRootHash << " stateDeltaHash "
                                  << microBlockStateDeltaHash << " : "
                                  << isEveryMicroBlockAvailable);
    return found;
}

bool Node::IsMicroBlockStateDeltaHashInFinalBlock(
    StateHash microBlockStateDeltaHash, TxnHash microBlockTxRootHash,
    const uint64_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Deleting unavailable "
                  << "microblock stateDeltaHash: " << microBlockStateDeltaHash
                  << " txRootHash " << microBlockTxRootHash << " for blocknum "
                  << blocknum);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    auto it = m_unavailableMicroBlocks.find(blocknum);
    bool found
        = (it != m_unavailableMicroBlocks.end()
           && RemoveStateDeltaHashFromUnavailableMicroBlock(
                  blocknum, microBlockStateDeltaHash, microBlockTxRootHash));
    isEveryMicroBlockAvailable = found && it->second.empty();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Found stateDeltaHash " << microBlockStateDeltaHash << ": "
                                      << isEveryMicroBlockAvailable);
    return found;
}

bool Node::LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                           const uint64_t& blocknum)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Unavailable microblock hashes in final block : ")

    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    for (uint i = 0; i < finalBlock.GetMicroBlockHashes().size(); ++i)
    {
        if (!finalBlock.GetIsMicroBlockEmpty()[i]
            || (finalBlock.GetMicroBlockHashes()[i].m_stateDeltaHash
                != StateHash()))
        {
            m_unavailableMicroBlocks[blocknum].insert(
                {{finalBlock.GetMicroBlockHashes()[i],
                  finalBlock.GetShardIDs()[i]},
#ifdef IS_LOOKUP_NODE
                 {!finalBlock.GetIsMicroBlockEmpty()[i], true}});
#else // IS_LOOKUP_NODE
                 {false, true}});
#endif // IS_LOOKUP_NODE
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      finalBlock.GetMicroBlockHashes()[i]);
        }
    }

#ifndef IS_LOOKUP_NODE
    if (m_unavailableMicroBlocks.find(blocknum)
            != m_unavailableMicroBlocks.end()
        && m_unavailableMicroBlocks[blocknum].size() > 0)
    {
        {
            lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
            m_allMicroBlocksRecvd = false;
        }

        bool doRejoin = false;

        if (IsMyShardMicroBlockInFinalBlock(blocknum))
        {
            {
                lock_guard<mutex> g3(m_mutexTempCommitted);
                m_tempStateDeltaCommitted = false;
            }
            if (m_lastMicroBlockCoSig.first != m_mediator.m_currentEpochNum)
            {
                LOG_GENERAL(WARNING,
                            "Found my microblock but Cosig not updated");
                doRejoin = true;
            }
        }
        else
        {
            if (IsMyShardIdInFinalBlock(blocknum))
            {
                LOG_GENERAL(WARNING,
                            "Didn't found my micorblock but found shard ID");
                doRejoin = true;
            }
        }

        if (doRejoin || m_doRejoinAtFinalBlock)
        {
            LOG_GENERAL(WARNING,
                        "Failed the last microblock consensus but "
                        "still found my shard microblock, "
                        " need to Rejoin");
            RejoinAsNormal();
            return false;
        }
    }
#endif //IS_LOOKUP_NODE
    return true;
}

bool Node::RemoveTxRootHashFromUnavailableMicroBlock(
    const uint64_t& blocknum, const TxnHash& txnRootHash,
    const StateHash& stateDeltaHash)
{
    for (auto it = m_unavailableMicroBlocks[blocknum].begin();
         it != m_unavailableMicroBlocks[blocknum].end(); it++)
    {
        if (it->first.m_hash.m_txRootHash == txnRootHash
            && it->first.m_hash.m_stateDeltaHash == stateDeltaHash)
        {
            LOG_GENERAL(INFO,
                        "Found microblock txnRootHash: " << txnRootHash
                                                         << " stateDeltaHash: "
                                                         << stateDeltaHash);
            it->second[0] = false;
            if (!it->second[1])
            {
                LOG_GENERAL(INFO,
                            "Remove microblock (txRootHash: "
                                << txnRootHash << " stateDeltaHash: "
                                << it->first.m_hash.m_stateDeltaHash << ")");
                LOG_GENERAL(INFO,
                            "Microblocks count before removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
                m_unavailableMicroBlocks[blocknum].erase(it);
                LOG_GENERAL(INFO,
                            "Microblocks count after removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
            }
            return true;
        }
    }
    LOG_GENERAL(WARNING,
                "Haven't found microblock txnRootHash: " << txnRootHash);
    return false;
}

bool Node::RemoveStateDeltaHashFromUnavailableMicroBlock(
    const uint64_t& blocknum, const StateHash& stateDeltaHash,
    const TxnHash& txnRootHash)
{
    for (auto it = m_unavailableMicroBlocks[blocknum].begin();
         it != m_unavailableMicroBlocks[blocknum].end(); it++)
    {
        if (it->first.m_hash.m_stateDeltaHash == stateDeltaHash
            && it->first.m_hash.m_txRootHash == txnRootHash)
        {
            LOG_GENERAL(INFO,
                        "Found microblock stateDeltaHash: " << stateDeltaHash
                                                            << " txnRootHash: "
                                                            << txnRootHash);
            it->second[1] = false;
            if (!it->second[0])
            {
                LOG_GENERAL(INFO,
                            "Remove microblock (txRootHash: "
                                << it->first.m_hash.m_txRootHash
                                << " stateDeltaHash: " << stateDeltaHash
                                << ")");
                LOG_GENERAL(INFO,
                            "Microblocks count before removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
                m_unavailableMicroBlocks[blocknum].erase(it);
                LOG_GENERAL(INFO,
                            "Microblocks count after removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
            }
            return true;
        }
    }
    LOG_GENERAL(WARNING,
                "Haven't found microblock stateDeltaHash: " << stateDeltaHash);
    return false;
}

bool Node::VerifyFinalBlockCoSignature(const TxBlock& txblock)
{
    LOG_MARKER();

    unsigned int index = 0;
    unsigned int count = 0;

    const vector<bool>& B2 = txblock.GetB2();
    if (m_mediator.m_DSCommittee.size() != B2.size())
    {
        LOG_GENERAL(WARNING,
                    "Mismatch: DS committee size = "
                        << m_mediator.m_DSCommittee.size()
                        << ", co-sig bitmap size = " << B2.size());
        return false;
    }

    // Generate the aggregated key
    vector<PubKey> keys;
    for (auto const& kv : m_mediator.m_DSCommittee)
    {
        if (B2.at(index) == true)
        {
            keys.emplace_back(kv.first);
            count++;
        }
        index++;
    }

    if (count != ConsensusCommon::NumForConsensus(B2.size()))
    {
        LOG_GENERAL(WARNING, "Cosig was not generated by enough nodes");
        return false;
    }

    shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
    if (aggregatedKey == nullptr)
    {
        LOG_GENERAL(WARNING, "Aggregated key generation failed");
        return false;
    }

    // Verify the collective signature
    vector<unsigned char> message;
    txblock.GetHeader().Serialize(message, 0);
    txblock.GetCS1().Serialize(message, TxBlockHeader::SIZE);
    BitVector::SetBitVector(message, TxBlockHeader::SIZE + BLOCK_SIG_SIZE,
                            txblock.GetB1());
    if (Schnorr::GetInstance().Verify(message, 0, message.size(),
                                      txblock.GetCS2(), *aggregatedKey)
        == false)
    {
        LOG_GENERAL(WARNING, "Cosig verification failed");
        for (auto& kv : keys)
        {
            LOG_GENERAL(WARNING, kv);
        }
        return false;
    }

    return true;
}

bool Node::CheckMicroBlockRootHash(const TxBlock& finalBlock,
                                   [[gnu::unused]] const uint64_t& blocknum)
{
    TxnHash microBlocksHash
        = ComputeTransactionsRoot(finalBlock.GetMicroBlockHashes());

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "Expected FinalBlock TxRoot hash : "
            << DataConversion::charArrToHexStr(microBlocksHash.asArray()));

    if (finalBlock.GetHeader().GetTxRootHash() != microBlocksHash)
    {
        LOG_GENERAL(INFO,
                    "TxRootHash in Final Block Header doesn't match root of "
                    "microblock hashes");
        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "FinalBlock TxRoot hash in final block by DS is correct");

    return true;
}

#ifndef IS_LOOKUP_NODE
void Node::CommitMyShardsMicroBlock()
{
    LOG_MARKER();

    // for (const auto& tx : m_committedTransactions)
    // {
    //     vector<unsigned char> serializedTxBody;
    //     tx.Serialize(serializedTxBody, 0);
    //     if (!BlockStorage::GetBlockStorage().PutTxBody(tx_hash,
    //                                                    serializedTxBody))
    //     {
    //         LOG_GENERAL(INFO, "FAIL: PutTxBody Failed");
    //     }
    // }
}

bool Node::IsMyShardMicroBlockTxRootHashInFinalBlock(
    const uint64_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    return m_microblock != nullptr
        && IsMicroBlockTxRootHashInFinalBlock(
               m_microblock->GetHeader().GetTxRootHash(),
               m_microblock->GetHeader().GetStateDeltaHash(), blocknum,
               isEveryMicroBlockAvailable);
}

bool Node::IsMyShardMicroBlockStateDeltaHashInFinalBlock(
    const uint64_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    return m_microblock != nullptr
        && IsMicroBlockStateDeltaHashInFinalBlock(
               m_microblock->GetHeader().GetStateDeltaHash(),
               m_microblock->GetHeader().GetTxRootHash(), blocknum,
               isEveryMicroBlockAvailable);
}

bool Node::IsMyShardMicroBlockInFinalBlock(const uint64_t& blocknum)
{
    if (m_microblock == nullptr)
    {
        return false;
    }

    auto it = m_unavailableMicroBlocks.find(blocknum);
    if (it == m_unavailableMicroBlocks.end())
    {
        return false;
    }

    for (auto it2 = m_unavailableMicroBlocks[blocknum].begin();
         it2 != m_unavailableMicroBlocks[blocknum].end(); it2++)
    {
        if (it2->first.m_hash.m_stateDeltaHash
                == m_microblock->GetHeader().GetStateDeltaHash()
            && it2->first.m_hash.m_txRootHash
                == m_microblock->GetHeader().GetTxRootHash())
        {
            LOG_GENERAL(INFO, "Found my shard microblock in finalblock");
            return true;
        }
    }

    LOG_GENERAL(WARNING, "Didn't find my shard microblock in finalblock");
    return false;
}

bool Node::IsMyShardIdInFinalBlock(const uint64_t& blocknum)
{
    auto it = m_unavailableMicroBlocks.find(blocknum);
    if (it == m_unavailableMicroBlocks.end())
    {
        return false;
    }

    for (auto it2 = m_unavailableMicroBlocks[blocknum].begin();
         it2 != m_unavailableMicroBlocks[blocknum].end(); it2++)
    {
        if (it2->first.m_shardID == m_myShardID)
        {
            LOG_GENERAL(INFO, "Found my shard ID in finalblock");
            return true;
        }
    }

    LOG_GENERAL(WARNING, "Didn't find my shard ID in finalblock");
    return false;
}

void Node::InitiatePoW()
{
    // reset consensusID and first consensusLeader is index 0
    m_consensusID = 0;
    m_consensusLeaderID = 0;

    SetState(POW_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Start pow ");
    auto func = [this]() mutable -> void {
        auto epochNumber
            = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1;
        auto dsBlockRand = m_mediator.m_dsBlockRand;
        auto txBlockRand = m_mediator.m_txBlockRand;
        StartPoW(epochNumber, POW_DIFFICULTY, dsBlockRand, txBlockRand);
    };

    DetachedFunction(1, func);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Soln to pow found ");
}

void Node::UpdateStateForNextConsensusRound()
{
    // Set state to tx submission
    if (m_isPrimary == true)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am no longer the shard leader ");
        m_isPrimary = false;
    }

    m_consensusLeaderID++;
    m_consensusID++;
    m_consensusLeaderID = m_consensusLeaderID % COMM_SIZE;

    if (m_consensusMyID == m_consensusLeaderID)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am the new shard leader ");
        m_isPrimary = true;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "The new shard leader is m_consensusMyID "
                      << m_consensusLeaderID);
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: Next non-ds epoch begins");

    SetState(TX_SUBMISSION);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[No PoW needed] MS: Start submit txn stage again.");
}

void Node::ScheduleTxnSubmission()
{
    auto main_func = [this]() mutable -> void { SubmitTransactions(); };

    DetachedFunction(1, main_func);

    LOG_GENERAL(INFO, "Sleep for " << TXN_SUBMISSION << " seconds");
    this_thread::sleep_for(chrono::seconds(TXN_SUBMISSION));
    LOG_GENERAL(INFO,
                "Woken up from the sleep of " << TXN_SUBMISSION << " seconds");
    auto main_func2
        = [this]() mutable -> void { SetState(TX_SUBMISSION_BUFFER); };

    DetachedFunction(1, main_func2);
}

void Node::ScheduleMicroBlockConsensus()
{
    LOG_GENERAL(INFO,
                "I am going to use conditional variable with timeout of  "
                    << TXN_BROADCAST << " seconds. It is ok to timeout here. ");
    std::unique_lock<std::mutex> cv_lk(m_MutexCVMicroblockConsensus);
    if (cv_microblockConsensus.wait_for(cv_lk,
                                        std::chrono::seconds(TXN_BROADCAST))
        == std::cv_status::timeout)
    {
        LOG_GENERAL(
            INFO, "Woken up from the sleep of " << TXN_BROADCAST << " seconds");
    }
    else
    {
        LOG_GENERAL(INFO,
                    "Received announcement message. Time to run consensus.");
    }
    auto main_func3 = [this]() mutable -> void { RunConsensusOnMicroBlock(); };

    DetachedFunction(1, main_func3);
}

void Node::BeginNextConsensusRound()
{
    LOG_MARKER();

    UpdateStateForNextConsensusRound();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    if (!isVacuousEpoch)
    {
        {
            unique_lock<mutex> g(m_mutexAllMicroBlocksRecvd);
            if (!m_allMicroBlocksRecvd)
            {
                LOG_GENERAL(INFO, "Wait for allMicroBlocksRecvd");
                if (m_cvAllMicroBlocksRecvd.wait_for(
                        g, std::chrono::seconds(TXN_SUBMISSION + TXN_BROADCAST))
                        == std::cv_status::timeout
                    || m_doRejoinAtNextRound)
                {
                    LOG_EPOCH(WARNING,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "Wake up from "
                                  << TXN_SUBMISSION + TXN_BROADCAST
                                  << "of waiting for all microblock received");
                    if (m_mediator.m_lookup->m_syncType == SyncType::NO_SYNC)
                    {
                        LOG_EPOCH(
                            WARNING,
                            to_string(m_mediator.m_currentEpochNum).c_str(),
                            "Not in rejoin mode, try rejoining as normal");
                        RejoinAsNormal();
                        return;
                    }
                }
                else
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "All microblocks recvd, moving to "
                              "ScheduleTxnSubmission");
                }
            }
            else
            {
                LOG_GENERAL(INFO, "No need to wait for allMicroBlocksRecvd");
            }

            {
                lock_guard<mutex> g2(m_mutexNewRoundStarted);
                if (!m_newRoundStarted)
                {
                    m_newRoundStarted = true;
                    m_cvNewRoundStarted.notify_all();
                }
            }
        }

        ScheduleTxnSubmission();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Vacuous epoch: Skipping submit transactions");
    }

    ScheduleMicroBlockConsensus();
}

void Node::CallActOnFinalBlock()
{
    LOG_MARKER();

    const TxBlock finalblock = m_mediator.m_txBlockChain.GetLastBlock();
    const uint64_t& blocknum = finalblock.GetHeader().GetBlockNum();

    // LoadUnavailableMicroBlockTxRootHashes(finalblock, blocknum);
    lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
    bool isEveryMicroBlockAvailable;

    // For now, since each sharding setup only processes one block, then whatever transactions we
    // failed to submit have to be discarded m_createdTransactions.clear();
    if (IsMyShardMicroBlockTxRootHashInFinalBlock(blocknum,
                                                  isEveryMicroBlockAvailable)
        && IsMyShardMicroBlockStateDeltaHashInFinalBlock(
               blocknum, isEveryMicroBlockAvailable))
    {
        vector<Transaction> txns_to_send;

        CommitMyShardsMicroBlock();

        {
            lock_guard<mutex> gt(m_mutexTempCommitted);
            AccountStore::GetInstance().CommitTemp();
            m_tempStateDeltaCommitted = true;

            LOG_GENERAL(INFO, "Temp State Committed");
        }

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(blocknum);
        }
    }
    else if (m_microblock != nullptr
             && m_microblock->GetHeader().GetNumTxs() > 0)
    {
        // TODO
        LOG_GENERAL(WARNING, "Why my shards microblock not in finalblock, one");
    }
}
#endif // IS_LOOKUP_NODE

void Node::LogReceivedFinalBlockDetails([[gnu::unused]] const TxBlock& txblock)
{
#ifdef IS_LOOKUP_NODE
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the lookup node have deserialized the TxBlock");
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "txblock.GetHeader().GetType(): " << txblock.GetHeader().GetType());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetVersion(): "
                  << txblock.GetHeader().GetVersion());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetGasLimit(): "
                  << txblock.GetHeader().GetGasLimit());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetGasUsed(): "
                  << txblock.GetHeader().GetGasUsed());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetBlockNum(): "
                  << txblock.GetHeader().GetBlockNum());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetNumMicroBlockHashes(): "
                  << txblock.GetHeader().GetNumMicroBlockHashes());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetStateRootHash(): "
                  << txblock.GetHeader().GetStateRootHash());
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "txblock.GetHeader().GetNumTxs(): " << txblock.GetHeader().GetNumTxs());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetMinerPubKey(): "
                  << txblock.GetHeader().GetMinerPubKey());
#endif // IS_LOOKUP_NODE
}

bool Node::CheckStateRoot(const TxBlock& finalBlock)
{
    StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();

    AccountStore::GetInstance().PrintAccountState();

    if (stateRoot != finalBlock.GetHeader().GetStateRootHash())
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State root doesn't match. Expected = "
                      << stateRoot << ". "
                      << "Received = "
                      << finalBlock.GetHeader().GetStateRootHash());
        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "State root matched "
                  << finalBlock.GetHeader().GetStateRootHash());

    return true;
}

// void Node::StoreMicroBlocksToDisk()
// {
//     LOG_MARKER();
//     for(auto microBlock : m_microblocks)
//     {

//         LOG_GENERAL(INFO,  "Storing Micro Block Hash: " << microBlock.GetHeader().GetTxRootHash() <<
//             " with Type: " << microBlock.GetHeader().GetType() <<
//             ", Version: " << microBlock.GetHeader().GetVersion() <<
//             ", Timestamp: " << microBlock.GetHeader().GetTimestamp() <<
//             ", NumTxs: " << microBlock.GetHeader().GetNumTxs());

//         vector<unsigned char> serializedMicroBlock;
//         microBlock.Serialize(serializedMicroBlock, 0);
//         BlockStorage::GetBlockStorage().PutMicroBlock(microBlock.GetHeader().GetTxRootHash(),
//                                                serializedMicroBlock);
//     }
//     m_microblocks.clear();
// }

bool Node::ProcessFinalBlock(const vector<unsigned char>& message,
                             unsigned int offset,
                             [[gnu::unused]] const Peer& from)
{
    // Message = [8-byte DS blocknum] [4-byte consensusid] [1-byte shard id]
    //           [Final block] [Tx body sharing setup]
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    if (m_lastMicroBlockCoSig.first != m_mediator.m_currentEpochNum)
    {
        std::unique_lock<mutex> cv_lk(m_MutexCVFBWaitMB);
        if (cv_FBWaitMB.wait_for(cv_lk, std::chrono::seconds(TXN_SUBMISSION))
            == std::cv_status::timeout)
        {
            LOG_GENERAL(WARNING,
                        "Timeout, I didn't finish microblock consensus");
        }
    }

    if (m_state == MICROBLOCK_CONSENSUS)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I may have missed the micrblock consensus. However, if I "
                  "recently received a valid finalblock, I will accept it");
        // TODO: Optimize state transition.
        AccountStore::GetInstance().InitTemp();
        SetState(WAITING_FINALBLOCK);
    }

    if (!CheckState(PROCESS_FINALBLOCK))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Too late - current state is " << m_state << ".");
        return false;
    }

#endif // IS_LOOKUP_NODE

    LOG_STATE(
        "[FLBLK]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] RECEIVED FINAL BLOCK");

    unsigned int cur_offset = offset;

    // Initialize it with maximum number
    uint32_t shard_id = std::numeric_limits<uint32_t>::max();

    // Reads and checks DS Block number, consensus ID and Shard ID
    if (!ReadAuxilliaryInfoFromFinalBlockMsg(message, cur_offset, shard_id))
    {
        return false;
    }

    // TxBlock txBlock(message, cur_offset);
    TxBlock txBlock;
    if (txBlock.Deserialize(message, cur_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize TxBlock.");
        return false;
    }
    cur_offset += txBlock.GetSerializedSize();

    LogReceivedFinalBlockDetails(txBlock);

    LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << txBlock.GetHeader().GetBlockNum()
                         << "] FRST");

    // Verify the co-signature
    if (!VerifyFinalBlockCoSignature(txBlock))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "TxBlock co-sig verification failed");
        return false;
    }

    // #ifdef IS_LOOKUP_NODE
    if (!CheckMicroBlockRootHash(txBlock, txBlock.GetHeader().GetBlockNum()))
    {
        return false;
    }

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (!isVacuousEpoch)
    {
        if (!LoadUnavailableMicroBlockHashes(txBlock,
                                             txBlock.GetHeader().GetBlockNum()))
        {
            return false;
        }
    }
    else
    {
        LOG_GENERAL(INFO, "isVacuousEpoch now");

        if (!AccountStore::GetInstance().UpdateStateTrieAll())
        {
            LOG_GENERAL(WARNING, "UpdateStateTrieAll Failed");
            return false;
        }

        if (!CheckStateRoot(txBlock)
#ifndef IS_LOOKUP_NODE
            || m_doRejoinAtStateRoot)
        {
            RejoinAsNormal();
#else // IS_LOOKUP_NODE
        )
        {
#endif // IS_LOOKUP_NODE
            return false;
        }
        StoreState();
        BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                    {'0'});
#ifndef IS_LOOKUP_NODE
        BlockStorage::GetBlockStorage().PopFrontTxBodyDB();
#else // IS_LOOKUP_NODE
        BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TX_BODY_TMP);
#endif // IS_LOOKUP_NODE
    }
    // #endif // IS_LOOKUP_NODE

    StoreFinalBlock(txBlock);

    if (txBlock.GetHeader().GetNumMicroBlockHashes() == 1)
    {
        LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << txBlock.GetHeader().GetBlockNum()
                             << "] LAST");
    }

    CommitForwardedMsgBuffer();

    // Assumption: New PoW done after every block committed
    // If I am not a DS committee member (and since I got this FinalBlock message,
    // then I know I'm not), I can start doing PoW again
    m_mediator.UpdateDSBlockRand();
    m_mediator.UpdateTxBlockRand();

#ifndef IS_LOOKUP_NODE
    CallActOnFinalBlock();

    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
    {
        InitiatePoW();
    }
    else
    {
        auto main_func
            = [this]() mutable -> void { BeginNextConsensusRound(); };

        DetachedFunction(1, main_func);
    }
#else // IS_LOOKUP_NODE
    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
    {
        m_consensusID = 0;
        m_consensusLeaderID = 0;
    }
    else
    {
        m_consensusID++;
        m_consensusLeaderID++;
        m_consensusLeaderID = m_consensusLeaderID % COMM_SIZE;
    }
#endif // IS_LOOKUP_NODE

    return true;
}

bool Node::LoadForwardedTxnsAndCheckRoot(
    const vector<unsigned char>& message, unsigned int cur_offset,
    TxnHash& microBlockTxHash, StateHash& microBlockStateDeltaHash,
    vector<Transaction>& txnsInForwardedMessage)
// vector<TxnHash> & txnHashesInForwardedMessage)
{
    LOG_MARKER();

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + TRAN_HASH_SIZE,
         microBlockTxHash.asArray().begin());
    cur_offset += TRAN_HASH_SIZE;

    LOG_GENERAL(
        INFO,
        "Received MicroBlock TxHash root : "
            << DataConversion::charArrToHexStr(microBlockTxHash.asArray()));

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + STATE_HASH_SIZE,
         microBlockStateDeltaHash.asArray().begin());
    cur_offset += STATE_HASH_SIZE;

    LOG_GENERAL(INFO,
                "Received MicroBlock StateDelta root : "
                    << DataConversion::charArrToHexStr(
                           microBlockStateDeltaHash.asArray()));

    vector<TxnHash> txnHashesInForwardedMessage;

    while (cur_offset < message.size())
    {
        // reading [Transaction] from received msg
        // Transaction tx(message, cur_offset);
        Transaction tx;
        if (tx.Deserialize(message, cur_offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize Transaction.");
            return false;
        }
        cur_offset += tx.GetSerializedSize();

        txnsInForwardedMessage.emplace_back(tx);
        txnHashesInForwardedMessage.emplace_back(tx.GetTranID());
    }

    return ComputeTransactionsRoot(txnHashesInForwardedMessage)
        == microBlockTxHash;
}

bool Node::LoadForwardedStateDeltaAndCheckRoot(
    const vector<unsigned char>& message, unsigned int cur_offset,
    StateHash& microBlockStateDeltaHash, TxnHash& microBlockTxHash)
{
    LOG_MARKER();

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + STATE_HASH_SIZE,
         microBlockStateDeltaHash.asArray().begin());
    cur_offset += STATE_HASH_SIZE;

    LOG_GENERAL(INFO,
                "Received MicroBlock State Delta root : "
                    << DataConversion::charArrToHexStr(
                           microBlockStateDeltaHash.asArray()));

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + TRAN_HASH_SIZE,
         microBlockTxHash.asArray().begin());
    cur_offset += TRAN_HASH_SIZE;

    LOG_GENERAL(
        INFO,
        "Received MicroBlock TxHash root : "
            << DataConversion::charArrToHexStr(microBlockTxHash.asArray()));

    vector<unsigned char> vec;
    copy(message.begin() + cur_offset, message.end(), back_inserter(vec));

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    StateHash delta(sha2.Finalize());

    LOG_GENERAL(INFO, "Calculated StateHash: " << delta);

    return delta == microBlockStateDeltaHash;
}

void Node::CommitForwardedTransactions(
    const vector<Transaction>& txnsInForwardedMessage, const uint64_t& blocknum)
{
    LOG_MARKER();

    unsigned int txn_counter = 0;
    for (const auto& tx : txnsInForwardedMessage)
    {
        {
            lock_guard<mutex> g(m_mutexCommittedTransactions);
            m_committedTransactions[blocknum].emplace_back(tx);
            // if (!AccountStore::GetInstance().UpdateAccounts(
            //         m_mediator.m_currentEpochNum - 1, tx))
            // {
            //     LOG_GENERAL(WARNING, "UpdateAccounts failed");
            //     m_committedTransactions[blocknum].pop_back();
            //     continue;
            // }
        }

            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //              "[TXN] [" << blocknum << "] Body received = 0x" << tx.GetTranID());

            // Update from and to accounts
            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Account store updated");

            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //              "Storing Transaction: " << tx.GetTranID() <<
            //              " with amount: " << tx.GetAmount() <<
            //              ", to: " << tx.GetToAddr() <<
            //              ", from: " << tx.GetFromAddr());
#ifdef IS_LOOKUP_NODE
        Server::AddToRecentTransactions(tx.GetTranID());
#endif //IS_LOOKUP_NODE

        // Store TxBody to disk
        vector<unsigned char> serializedTxBody;
        tx.Serialize(serializedTxBody, 0);
        BlockStorage::GetBlockStorage().PutTxBody(tx.GetTranID(),
                                                  serializedTxBody);

        txn_counter++;
        if (txn_counter % 10000 == 0)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Proceessed " << txn_counter << " of txns.");
        }
    }
}

#ifndef IS_LOOKUP_NODE
void Node::LoadFwdingAssgnForThisBlockNum(const uint64_t& blocknum,
                                          vector<Peer>& forward_list)
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexForwardingAssignment);
    auto f = m_forwardingAssignment.find(blocknum);
    if (f != m_forwardingAssignment.end())
    {
        forward_list = f->second;
    }
}
#endif // IS_LOOKUP_NODE

void Node::DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
    const uint64_t& blocknum)
{
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    lock(m_mutexForwardingAssignment, m_mutexUnavailableMicroBlocks);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks, adopt_lock);
    lock_guard<mutex> g2(m_mutexForwardingAssignment, adopt_lock);
#else // IS_LOOKUP_NODE
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
#endif // IS_LOOKUP_NODE

    auto it = m_unavailableMicroBlocks.find(blocknum);

    for (auto it : m_unavailableMicroBlocks)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unavailable"
                  " microblock bodies in finalblock "
                      << it.first << ": " << it.second.size());
        for (auto it2 : it.second)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      it2.first);
        }
    }

    if (it != m_unavailableMicroBlocks.end() && it->second.empty())
    {
        m_unavailableMicroBlocks.erase(it);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Deleting blocknum "
                      << blocknum << " from unavailable microblocks list.");

#ifndef IS_LOOKUP_NODE
        m_forwardingAssignment.erase(blocknum);
        lock_guard<mutex> gt(m_mutexTempCommitted);
        if (m_unavailableMicroBlocks.empty() && m_tempStateDeltaCommitted)
        {
            {
                lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
                m_allMicroBlocksRecvd = true;
            }
            LOG_GENERAL(INFO, "Notify All MicroBlocks Received");
            m_cvAllMicroBlocksRecvd.notify_all();
        }
#endif // IS_LOOKUP_NODE
        LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << blocknum << "] LAST");
    }
}

bool Node::ProcessForwardTransaction(const vector<unsigned char>& message,
                                     unsigned int cur_offset,
                                     [[gnu::unused]] const Peer& from)
{
    // Message = [block number] [microblocktxhash] [microblockdeltahash] [Transaction] [Transaction] [Transaction] ....
    // Received from other shards

    LOG_MARKER();

    // reading [block number] from received msg
    uint64_t latestForwardBlockNum = Serializable::GetNumber<uint64_t>(
        message, cur_offset, sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    LOG_STATE(
        "[TXBOD]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] RECEIVED TXN BODIES #" << latestForwardBlockNum);

    LOG_GENERAL(INFO,
                "Received forwarded txns for block number "
                    << latestForwardBlockNum);

    if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        < latestForwardBlockNum)
    {
        vector<unsigned char> txnMsg;
        copy(message.begin() + cur_offset, message.end(),
             back_inserter(txnMsg));

        lock_guard<mutex> g(m_mutexForwardedTxnBuffer);
        m_forwardedTxnBuffer[latestForwardBlockNum].push_back(txnMsg);

        return true;
    }
    else if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
             == latestForwardBlockNum)
    {
        return ProcessForwardTransactionCore(message, cur_offset);
    }

    LOG_GENERAL(WARNING,
                "Current block num: "
                    << m_mediator.m_txBlockChain.GetLastBlock()
                           .GetHeader()
                           .GetBlockNum()
                    << " this forwarded delta msg is too late");

    return false;
}

bool Node::ProcessForwardTransactionCore(const vector<unsigned char>& message,
                                         unsigned int cur_offset)
{
    TxnHash microBlockTxRootHash;
    StateHash microBlockStateDeltaHash;
    vector<Transaction> txnsInForwardedMessage;
    // vector<TxnHash> txnHashesInForwardedMessage;

    if (!LoadForwardedTxnsAndCheckRoot(
            message, cur_offset, microBlockTxRootHash, microBlockStateDeltaHash,
            txnsInForwardedMessage /*, txnHashesInForwardedMessage*/))
    {
        return false;
    }

    {
        lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
        bool isEveryMicroBlockAvailable;

        if (!IsMicroBlockTxRootHashInFinalBlock(
                microBlockTxRootHash, microBlockStateDeltaHash,
                m_mediator.m_txBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum(),
                isEveryMicroBlockAvailable))
        {
            LOG_GENERAL(WARNING,
                        "The forwarded data is not in finalblock, why?");
            return false;
        }
        // StoreTxInMicroBlock(microBlockTxRootHash, txnHashesInForwardedMessage)

        CommitForwardedTransactions(
            txnsInForwardedMessage,
            m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum());

#ifndef IS_LOOKUP_NODE
        vector<Peer> forward_list;
        LoadFwdingAssgnForThisBlockNum(
            m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
            forward_list);
#endif // IS_LOOKUP_NODE

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
                m_mediator.m_txBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum());
        }

#ifndef IS_LOOKUP_NODE
        if (forward_list.size() > 0)
        {
            P2PComm::GetInstance().SendBroadcastMessage(forward_list, message);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DEBUG I have broadcasted the txn body!");
        }
#endif // IS_LOOKUP_NODE
    }

    return true;
}

bool Node::ProcessForwardStateDelta(const vector<unsigned char>& message,
                                    unsigned int cur_offset,
                                    [[gnu::unused]] const Peer& from)
{
    // Message = [block number] [microblockdeltahash] [microblocktxhash] [AccountStateDelta]
    // Received from other shards

    LOG_MARKER();

    // reading [block number] from received msg
    uint64_t latestForwardBlockNum = Serializable::GetNumber<uint64_t>(
        message, cur_offset, sizeof(uint64_t));

    cur_offset += sizeof(uint64_t);

    LOG_GENERAL(INFO,
                "Received state delta for block number "
                    << latestForwardBlockNum);

    if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        < latestForwardBlockNum)
    {
        vector<unsigned char> deltaMsg;
        copy(message.begin() + cur_offset, message.end(),
             back_inserter(deltaMsg));

        lock_guard<mutex> g(m_mutexForwardedDeltaBuffer);
        m_forwardedDeltaBuffer[latestForwardBlockNum].push_back(deltaMsg);

        return true;
    }
    else if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
             == latestForwardBlockNum)
    {
        return ProcessForwardStateDeltaCore(message, cur_offset);
    }

    LOG_GENERAL(WARNING,
                "Current block num: "
                    << m_mediator.m_txBlockChain.GetLastBlock()
                           .GetHeader()
                           .GetBlockNum()
                    << " this forwarded delta msg is too late");

    return false;
}

bool Node::ProcessForwardStateDeltaCore(
    const std::vector<unsigned char>& message, unsigned int cur_offset)
{
    StateHash microBlockStateDeltaHash;
    TxnHash microBlockTxRootHash;

    if (!LoadForwardedStateDeltaAndCheckRoot(message, cur_offset,
                                             microBlockStateDeltaHash,
                                             microBlockTxRootHash))
    {
        return false;
    }

    LOG_GENERAL(INFO, "LoadedForwardedStateDelta!");

    cur_offset += STATE_HASH_SIZE + TRAN_HASH_SIZE;

    {
        lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
        bool isEveryMicroBlockAvailable;

        if (!IsMicroBlockStateDeltaHashInFinalBlock(
                microBlockStateDeltaHash, microBlockTxRootHash,
                m_mediator.m_txBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum(),
                isEveryMicroBlockAvailable))
        {
            LOG_GENERAL(WARNING,
                        "The forwarded data is not in finalblock, why?");
            return false;
        }

        AccountStore::GetInstance().DeserializeDelta(message, cur_offset);

#ifndef IS_LOOKUP_NODE
        vector<Peer> forward_list;
        LoadFwdingAssgnForThisBlockNum(
            m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
            forward_list);
#endif // IS_LOOKUP_NODE

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
                m_mediator.m_txBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum());
        }

#ifndef IS_LOOKUP_NODE
        if (forward_list.size() > 0)
        {
            P2PComm::GetInstance().SendBroadcastMessage(forward_list, message);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DEBUG I have broadcasted the state delta!");
        }
#endif // IS_LOOKUP_NODE
    }

    return true;
}

void Node::CommitForwardedMsgBuffer()
{
    {
        lock_guard<mutex> g(m_mutexForwardedTxnBuffer);

        for (auto it = m_forwardedTxnBuffer.begin();
             it != m_forwardedTxnBuffer.end();)
        {
            if (it->first < m_mediator.m_txBlockChain.GetLastBlock()
                                .GetHeader()
                                .GetBlockNum())
            {
                it = m_forwardedTxnBuffer.erase(it);
            }
            else if (it->first
                     == m_mediator.m_txBlockChain.GetLastBlock()
                            .GetHeader()
                            .GetBlockNum())
            {
                for (const auto& msg : it->second)
                {
                    ProcessForwardTransactionCore(msg, 0);
                }
                m_forwardedTxnBuffer.erase(it);
                break;
            }
            else
            {
                it++;
            }
        }
    }

    {
        lock_guard<mutex> g(m_mutexForwardedDeltaBuffer);

        for (auto it = m_forwardedDeltaBuffer.begin();
             it != m_forwardedDeltaBuffer.end();)
        {
            if (it->first < m_mediator.m_txBlockChain.GetLastBlock()
                                .GetHeader()
                                .GetBlockNum())
            {
                it = m_forwardedDeltaBuffer.erase(it);
            }
            else if (it->first
                     == m_mediator.m_txBlockChain.GetLastBlock()
                            .GetHeader()
                            .GetBlockNum())
            {
                for (const auto& msg : it->second)
                {
                    ProcessForwardStateDeltaCore(msg, 0);
                }
                m_forwardedDeltaBuffer.erase(it);
                break;
            }
            else
            {
                it++;
            }
        }
    }
}