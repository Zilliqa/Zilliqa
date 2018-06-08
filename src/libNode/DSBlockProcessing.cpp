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
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

void Node::StoreDSBlockToDisk(const DSBlock& dsblock)
{
    LOG_MARKER();

    m_mediator.m_dsBlockChain.AddBlock(dsblock);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Storing DS Block Number: "
                  << dsblock.GetHeader().GetBlockNum()
                  << " with Nonce: " << dsblock.GetHeader().GetNonce()
                  << ", Difficulty: " << dsblock.GetHeader().GetDifficulty()
                  << ", Timestamp: " << dsblock.GetHeader().GetTimestamp()
                  << ", view change count: "
                  << dsblock.GetHeader().GetViewChangeCount());

    // Update the rand1 value for next PoW
    m_mediator.UpdateDSBlockRand();

    // Store DS Block to disk
    vector<unsigned char> serializedDSBlock;
    dsblock.Serialize(serializedDSBlock, 0);

    LOG_GENERAL(
        INFO,
        "View change count:  " << dsblock.GetHeader().GetViewChangeCount());

    for (unsigned int i = 0; i < dsblock.GetHeader().GetViewChangeCount(); i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();
        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();
    }
    BlockStorage::GetBlockStorage().PutDSBlock(
        dsblock.GetHeader().GetBlockNum(), serializedDSBlock);
#ifndef IS_LOOKUP_NODE
    BlockStorage::GetBlockStorage().PushBackTxBodyDB(
        dsblock.GetHeader().GetBlockNum());
#endif
}

void Node::UpdateDSCommiteeComposition(const Peer& winnerpeer)
{
    LOG_MARKER();

    // Update my view of the DS committee
    // 1. Insert new leader at the head of the queue
    // 2. Pop out the oldest backup from the tail of the queue
    // Note: If I am the primary, push a placeholder with ip=0 and port=0 in place of my real port
    if (m_mediator.m_selfKey.second
        == m_mediator.m_dsBlockChain.GetLastBlock()
               .GetHeader()
               .GetMinerPubKey())
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_front(Peer());
    }
    else
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_front(winnerpeer);
    }
    m_mediator.m_DSCommitteePubKeys.push_front(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetMinerPubKey());

    m_mediator.m_DSCommitteeNetworkInfo.pop_back();
    m_mediator.m_DSCommitteePubKeys.pop_back();
}

bool Node::CheckWhetherDSBlockNumIsLatest(const uint256_t dsblockNum)
{
    LOG_MARKER();

    uint256_t latestBlockNumInBlockchain
        = m_mediator.m_dsBlockChain.GetBlockCount();

    if (dsblockNum < latestBlockNumInBlockchain)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "We are processing duplicated blocks");
        return false;
    }
    else if (dsblockNum > latestBlockNumInBlockchain)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Warning: We are missing of some DS blocks. Requested: "
                      << dsblockNum
                      << " while Present: " << latestBlockNumInBlockchain);
        // Todo: handle missing DS blocks.
        return false;
    }

    return true;
}

bool Node::VerifyDSBlockCoSignature(const DSBlock& dsblock)
{
    LOG_MARKER();

    unsigned int index = 0;
    unsigned int count = 0;

    const vector<bool>& B2 = dsblock.GetB2();
    if (m_mediator.m_DSCommitteePubKeys.size() != B2.size())
    {
        LOG_GENERAL(WARNING,
                    "Mismatch: DS committee size = "
                        << m_mediator.m_DSCommitteePubKeys.size()
                        << ", co-sig bitmap size = " << B2.size());
        return false;
    }

    // Generate the aggregated key
    vector<PubKey> keys;
    for (auto& kv : m_mediator.m_DSCommitteePubKeys)
    {
        if (B2.at(index) == true)
        {
            keys.push_back(kv);
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
    dsblock.GetHeader().Serialize(message, 0);
    dsblock.GetCS1().Serialize(message, DSBlockHeader::SIZE);
    BitVector::SetBitVector(message, DSBlockHeader::SIZE + BLOCK_SIG_SIZE,
                            dsblock.GetB1());
    if (Schnorr::GetInstance().Verify(message, 0, message.size(),
                                      dsblock.GetCS2(), *aggregatedKey)
        == false)
    {
        LOG_GENERAL(WARNING, "Cosig verification failed");
        return false;
    }

    return true;
}

void Node::LogReceivedDSBlockDetails(const DSBlock& dsblock)
{
#ifdef IS_LOOKUP_NODE
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the lookup node have deserialized the DS Block");
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetDifficulty(): "
                  << (int)dsblock.GetHeader().GetDifficulty());
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "dsblock.GetHeader().GetNonce(): " << dsblock.GetHeader().GetNonce());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetBlockNum(): "
                  << dsblock.GetHeader().GetBlockNum());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetMinerPubKey(): "
                  << dsblock.GetHeader().GetMinerPubKey());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetLeaderPubKey(): "
                  << dsblock.GetHeader().GetLeaderPubKey());
#endif // IS_LOOKUP_NODE
}

bool Node::ProcessDSBlock(const vector<unsigned char>& message,
                          unsigned int cur_offset, const Peer& from)
{
    // Message = [259-byte DS block] [32-byte DS block hash / rand1] [16-byte winner IP] [4-byte winner port]
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    // Checks if (m_state == POW2_SUBMISSION)
    if (!CheckState(STARTPOW2))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in POW2_SUBMISSION state");
        return false;
    }

    // For running from genesis
    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
    {
        m_mediator.m_lookup->m_syncType = SyncType::NO_SYNC;
        if (m_fromNewProcess)
        {
            m_fromNewProcess = false;
        }
    }
#else
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the lookup node have received the DS Block");
#endif // IS_LOOKUP_NODE

    if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                   DSBlock::GetMinSize() + BLOCK_HASH_SIZE))
    {
        return false;
    }

    // 259-byte DS block
    // DSBlock dsblock(message, cur_offset);
    DSBlock dsblock;
    if (dsblock.Deserialize(message, cur_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize dsblock.");
        return false;
    }

    cur_offset += dsblock.GetSerializedSize();

    LogReceivedDSBlockDetails(dsblock);

    // Checking for freshness of incoming DS Block
    if (!CheckWhetherDSBlockNumIsLatest(dsblock.GetHeader().GetBlockNum()))
    {
        return false;
    }

    // Check the signature of this DS block
    if (!VerifyDSBlockCoSignature(dsblock))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DSBlock co-sig verification failed");
        return false;
    }

    // 32-byte DS block hash / rand1
    array<unsigned char, BLOCK_HASH_SIZE> dsblockhash;
    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + BLOCK_HASH_SIZE, dsblockhash.begin());
    cur_offset += BLOCK_HASH_SIZE;

    // To-do: Verify the hash / rand1 value (if necessary)

    // 16-byte winner IP and 4-byte winner port
    Peer newleaderIP(message, cur_offset);
    cur_offset += (IP_SIZE + PORT_SIZE);

    // Add to block chain and Store the DS block to disk.
    StoreDSBlockToDisk(dsblock);
#ifdef IS_LOOKUP_NODE
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the lookup node have stored the DS Block");
#endif // IS_LOOKUP_NODE

    m_mediator.UpdateDSBlockRand(); // Update the rand1 value for next PoW
    UpdateDSCommiteeComposition(newleaderIP);

#ifndef IS_LOOKUP_NODE
    // Check if I am the next DS leader
    if (m_mediator.m_selfKey.second
        == m_mediator.m_dsBlockChain.GetLastBlock()
               .GetHeader()
               .GetMinerPubKey())
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I won PoW1 :-) I am now the new DS committee leader!");
        m_mediator.m_lookup->m_syncType = SyncType::NO_SYNC;
        m_mediator.m_ds->m_consensusMyID = 0;
        m_mediator.m_ds->m_consensusID
            = m_mediator.m_currentEpochNum == 1 ? 1 : 0;
        m_mediator.m_ds->SetState(DirectoryService::DirState::POW2_SUBMISSION);
        m_mediator.m_ds->NotifyPOW2Submission();
        m_mediator.m_ds->m_mode = DirectoryService::Mode::PRIMARY_DS;
        LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                      DS_LEADER_MSG);
#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][0     ] DSLD");
#endif // STAT_TEST
        m_mediator.m_ds->ScheduleShardingConsensus(
            LEADER_POW2_WINDOW_IN_SECONDS);
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I lost PoW1 :-( Better luck next time!");
        POW::GetInstance().StopMining();

        // Tell my Node class to start PoW2 if I didn't win PoW1
        array<unsigned char, 32> rand2 = {};
        StartPoW2(
            m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
            POW2_DIFFICULTY, m_mediator.m_dsBlockRand, rand2);
    }
#endif // IS_LOOKUP_NODE

    return true;
}
