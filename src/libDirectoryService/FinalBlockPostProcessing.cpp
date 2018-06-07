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
void DirectoryService::StoreFinalBlockToDisk()
{
    LOG_MARKER();

    // Add finalblock to txblockchain
    m_mediator.m_txBlockChain.AddBlock(*m_finalBlock);
    m_mediator.m_currentEpochNum
        = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();

    // At this point, the transactions in the last Epoch is no longer useful, thus erase.
    m_mediator.m_node->EraseCommittedTransactions(m_mediator.m_currentEpochNum
                                                  - 2);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Storing Tx Block Number: "
                  << m_finalBlock->GetHeader().GetBlockNum()
                  << " with Type: " << m_finalBlock->GetHeader().GetType()
                  << ", Version: " << m_finalBlock->GetHeader().GetVersion()
                  << ", Timestamp: " << m_finalBlock->GetHeader().GetTimestamp()
                  << ", NumTxs: " << m_finalBlock->GetHeader().GetNumTxs());

    vector<unsigned char> serializedTxBlock;
    m_finalBlock->Serialize(serializedTxBlock, 0);
    BlockStorage::GetBlockStorage().PutTxBlock(
        m_finalBlock->GetHeader().GetBlockNum(), serializedTxBlock);
}

bool DirectoryService::SendFinalBlockToLookupNodes()
{
    vector<unsigned char> finalblock_message
        = {MessageType::NODE, NodeInstructionType::FINALBLOCK};
    finalblock_message.resize(finalblock_message.size() + UINT256_SIZE
                              + sizeof(uint32_t) + sizeof(uint8_t)
                              + m_finalBlockMessage.size());

    unsigned char curr_offset = MessageOffset::BODY;

    // 32-byte DS blocknum
    uint256_t dsBlockNum = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
    Serializable::SetNumber<uint256_t>(finalblock_message, curr_offset,
                                       dsBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    // 4-byte consensusid
    Serializable::SetNumber<uint32_t>(finalblock_message, curr_offset,
                                      m_consensusID, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // randomly setting shard id to 0 -- shouldn't matter
    Serializable::SetNumber<uint8_t>(finalblock_message, curr_offset,
                                     (uint8_t)0, sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    copy(m_finalBlockMessage.begin(), m_finalBlockMessage.end(),
         finalblock_message.begin() + curr_offset);

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I the primary DS am sending the Final Block to the lookup nodes");
    m_mediator.m_lookup->SendMessageToLookupNodes(finalblock_message);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the primary DS have sent the Final Block to the lookup nodes");

    return true;
}

void DirectoryService::DetermineShardsToSendFinalBlockTo(
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

void DirectoryService::SendFinalBlockToShardNodes(
    unsigned int my_DS_cluster_num, unsigned int my_shards_lo,
    unsigned int my_shards_hi)
{
    // Too few target shards - avoid asking all DS clusters to send
    LOG_MARKER();

    if ((my_DS_cluster_num + 1) <= m_shards.size())
    {
        vector<unsigned char> finalblock_message
            = {MessageType::NODE, NodeInstructionType::FINALBLOCK};
        finalblock_message.resize(finalblock_message.size() + UINT256_SIZE
                                  + sizeof(uint32_t) + sizeof(uint8_t)
                                  + m_finalBlockMessage.size());

        copy(m_finalBlockMessage.begin(), m_finalBlockMessage.end(),
             finalblock_message.begin() + MessageOffset::BODY + UINT256_SIZE
                 + sizeof(uint32_t) + sizeof(uint8_t));

        unsigned char curr_offset = MessageOffset::BODY;

        // 32-byte DS blocknum
        uint256_t DSBlockNum = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
        Serializable::SetNumber<uint256_t>(finalblock_message, curr_offset,
                                           DSBlockNum, UINT256_SIZE);
        curr_offset += UINT256_SIZE;

        // 4-byte consensusid
        Serializable::SetNumber<uint32_t>(finalblock_message, curr_offset,
                                          m_consensusID, sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

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

            // Modify the shard id part of the message
            Serializable::SetNumber<uint8_t>(finalblock_message, curr_offset,
                                             (uint8_t)i, sizeof(uint8_t));

#ifdef STAT_TEST
            SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
            sha256.Update(finalblock_message);
            vector<unsigned char> this_msg_hash = sha256.Finalize();
            LOG_STATE(
                "[INFOR]["
                << setw(15) << left
                << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
                << "]["
                << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
                       .substr(0, 6)
                << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                << "] FBBLKGEN");
#endif // STAT_TEST

            P2PComm::GetInstance().SendBroadcastMessage(shard_peers,
                                                        finalblock_message);

            p++;
        }
    }

    m_finalBlockMessage.clear();
}

// void DirectoryService::StoreMicroBlocksToDisk()
// {
//     LOG_MARKER();
//     for(auto microBlock : m_microBlocks)
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
//     m_microBlocks.clear();
// }

void DirectoryService::ProcessFinalBlockConsensusWhenDone()
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block consensus is DONE!!!");

#ifdef STAT_TEST
    if (m_mode == PRIMARY_DS)
    {
        LOG_STATE("[FBCON]["
                  << setw(15) << left
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                  << m_mediator.m_txBlockChain.GetBlockCount() << "] DONE");
    }
#endif // STAT_TEST

    // Update the final block with the co-signatures from the consensus
    m_finalBlock->SetCoSignatures(*m_consensusObject);

    // Update m_finalBlockMessage too
    unsigned int cosigOffset = m_finalBlock->GetSerializedSize()
        - ((BlockBase)(*m_finalBlock)).GetSerializedSize();
    ((BlockBase)(*m_finalBlock)).Serialize(m_finalBlockMessage, cosigOffset);

    // StoreMicroBlocksToDisk();
    StoreFinalBlockToDisk();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (isVacuousEpoch)
    {
        if (CheckStateRoot())
        {
            AccountStore::GetInstance().MoveUpdatesToDisk();
            BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                        {'0'});
#ifndef IS_LOOKUP_NODE
            BlockStorage::GetBlockStorage().PopFrontTxBodyDB();
#endif // IS_LOOKUP_NODE
        }
    }

    m_mediator.UpdateDSBlockRand();
    m_mediator.UpdateTxBlockRand();

    // TODO: Refine this
    unsigned int nodeToSendToLookUpLo = COMM_SIZE / 4;
    unsigned int nodeToSendToLookUpHi
        = nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

    if (m_consensusMyID > nodeToSendToLookUpLo
        && m_consensusMyID < nodeToSendToLookUpHi)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I the DS folks that will soon be sending the Final Block to "
                  "the lookup nodes");
        SendFinalBlockToLookupNodes();
    }

    uint8_t tx_sharing_mode
        = (m_sharingAssignment.size() > 0) ? DS_FORWARD_ONLY : ::IDLE;
    m_mediator.m_node->ActOnFinalBlock(tx_sharing_mode, m_sharingAssignment);

    m_sharingAssignment.clear();

    unsigned int my_DS_cluster_num;
    unsigned int my_shards_lo;
    unsigned int my_shards_hi;

    DetermineShardsToSendFinalBlockTo(my_DS_cluster_num, my_shards_lo,
                                      my_shards_hi);
    SendFinalBlockToShardNodes(my_DS_cluster_num, my_shards_lo, my_shards_hi);

    m_allPoWConns.clear();

    // Assumption for now: New round of PoW done after every final block
    // Reset state to be ready to accept new PoW1 submissions
    SetState(POW1_SUBMISSION);
    cv_POW1Submission.notify_all();

    auto func = [this]() mutable -> void {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "START OF a new EPOCH");
        if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "[PoW needed]");

            POW::GetInstance().EthashConfigureLightClient(
                (uint64_t)m_mediator.m_dsBlockChain
                    .GetBlockCount()); // hack hack hack -- typecasting
            m_consensusID = 0;
            m_mediator.m_node->m_consensusID = 0;
            m_mediator.m_node->m_consensusLeaderID = 0;
            if (m_mode == PRIMARY_DS)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting "
                              << POW1_WINDOW_IN_SECONDS
                              << " seconds, accepting PoW1 submissions...");
                this_thread::sleep_for(chrono::seconds(POW1_WINDOW_IN_SECONDS));
                RunConsensusOnDSBlock();
            }
            else
            {
                std::unique_lock<std::mutex> cv_lk(m_MutexCVDSBlockConsensus);

                if (cv_DSBlockConsensus.wait_for(
                        cv_lk,
                        std::chrono::seconds(POW1_BACKUP_WINDOW_IN_SECONDS))
                    == std::cv_status::timeout)
                {
                    LOG_GENERAL(INFO,
                                "I have woken up from the sleep of "
                                    << POW1_BACKUP_WINDOW_IN_SECONDS
                                    << " seconds");
                }
                else
                {
                    LOG_GENERAL(INFO,
                                "I have received announcement message. Time to "
                                "run consensus.");
                }

                RunConsensusOnDSBlock();
                cv_DSBlockConsensusObject.notify_all();
            }
        }
        else
        {
            m_consensusID++;
            SetState(MICROBLOCK_SUBMISSION);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "[No PoW needed] Waiting for Microblock.");

            std::unique_lock<std::mutex> cv_lk(
                m_MutexScheduleFinalBlockConsensus);
            if (cv_scheduleFinalBlockConsensus.wait_for(
                    cv_lk, std::chrono::seconds(MICROBLOCK_TIMEOUT))
                == std::cv_status::timeout)
            {
                LOG_GENERAL(INFO,
                            "Timeout: Didn't receive all Microblock. Proceeds "
                            "without it");

                RunConsensusOnFinalBlock();
            }
        }
    };

    DetachedFunction(1, func);
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessFinalBlockConsensus(
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
    if ((m_state == MICROBLOCK_SUBMISSION)
        || (m_state == FINALBLOCK_CONSENSUS_PREP))
    {
        std::unique_lock<std::mutex> cv_lkObject(
            m_MutexCVFinalBlockConsensusObject);

        if (cv_finalBlockConsensusObject.wait_for(
                cv_lkObject,
                std::chrono::seconds(FINALBLOCK_CONSENSUS_OBJECT_TIMEOUT))
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

    if (!CheckState(PROCESS_FINALBLOCKCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Ignoring consensus message. I am at state " << m_state);
        return false;
    }

    bool result = m_consensusObject->ProcessMessage(message, offset, from);

    ConsensusCommon::State state = m_consensusObject->GetState();

    if (state == ConsensusCommon::State::DONE)
    {
        cv_viewChangeFinalBlock.notify_all();
        m_viewChangeCounter = 0;
        ProcessFinalBlockConsensusWhenDone();
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");
        // throw exception();
        // TODO: no consensus reached
        if (m_mode != PRIMARY_DS)
        {
            RejoinAsDS();
        }
        return false;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << state);
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}