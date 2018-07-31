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
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

bool Node::LoadShardingStructure(const vector<unsigned char>& message,
                                 unsigned int& cur_offset)
{
    vector<map<PubKey, Peer>> shards;
    cur_offset = ShardingStructure::Deserialize(message, cur_offset, shards);
    m_numShards = shards.size();

    // Check the shard ID against the deserialized structure
    if (m_myShardID >= shards.size())
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Shard ID " << m_myShardID << " >= num shards "
                              << shards.size());
        return false;
    }

    const map<PubKey, Peer>& my_shard = shards.at(m_myShardID);

    m_myShardMembersPubKeys.clear();
    m_myShardMembersNetworkInfo.clear();

    // All nodes; first entry is leader
    unsigned int index = 0;
    for (const auto& i : my_shard)
    {
        m_myShardMembersPubKeys.emplace_back(i.first);
        m_myShardMembersNetworkInfo.emplace_back(i.second);

        // Zero out my IP to avoid sending to myself
        if (m_mediator.m_selfPeer == m_myShardMembersNetworkInfo.back())
        {
            m_consensusMyID = index; // Set my ID
            m_myShardMembersNetworkInfo.back().m_listenPortHost = 0;
        }

        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            " PubKey: "
                << DataConversion::SerializableToHexStr(
                       m_myShardMembersPubKeys.back())
                << " IP: "
                << m_myShardMembersNetworkInfo.back().GetPrintableIPAddress()
                << " Port: "
                << m_myShardMembersNetworkInfo.back().m_listenPortHost);

        index++;
    }

    return true;
}

void Node::LoadTxnSharingInfo(const vector<unsigned char>& message,
                              unsigned int cur_offset)
{
    LOG_MARKER();

    m_txnSharingIAmSender = false;
    m_txnSharingIAmForwarder = false;
    m_txnSharingAssignedNodes.clear();

    vector<Peer> ds_receivers;
    vector<vector<Peer>> shard_receivers;
    vector<vector<Peer>> shard_senders;

    TxnSharingAssignments::Deserialize(message, cur_offset, ds_receivers,
                                       shard_receivers, shard_senders);

    // m_txnSharingAssignedNodes below is basically just the combination of ds_receivers, shard_receivers, and shard_senders
    // We will get rid of this inefficiency eventually

    m_txnSharingAssignedNodes.emplace_back();

    for (unsigned int i = 0; i < ds_receivers.size(); i++)
    {
        m_txnSharingAssignedNodes.back().emplace_back(ds_receivers.at(i));
    }

    for (unsigned int i = 0; i < shard_receivers.size(); i++)
    {
        m_txnSharingAssignedNodes.emplace_back();

        for (unsigned int j = 0; j < shard_receivers.at(i).size(); j++)
        {
            m_txnSharingAssignedNodes.back().emplace_back(
                shard_receivers.at(i).at(j));

            if ((i == m_myShardID)
                && (m_txnSharingAssignedNodes.back().back()
                    == m_mediator.m_selfPeer))
            {
                m_txnSharingIAmForwarder = true;
            }
        }

        m_txnSharingAssignedNodes.emplace_back();

        for (unsigned int j = 0; j < shard_senders.at(i).size(); j++)
        {
            m_txnSharingAssignedNodes.back().emplace_back(
                shard_senders.at(i).at(j));

            if ((i == m_myShardID)
                && (m_txnSharingAssignedNodes.back().back()
                    == m_mediator.m_selfPeer))
            {
                m_txnSharingIAmSender = true;
            }
        }
    }
}

bool Node::ProcessSharding([[gnu::unused]] const vector<unsigned char>& message,
                           [[gnu::unused]] unsigned int offset,
                           [[gnu::unused]] const Peer& from)
{
#ifndef IS_LOOKUP_NODE

    // Message = [8-byte DS blocknum] [4-byte shard ID] [Sharding structure] [Txn sharing assignments]

    LOG_MARKER();

    if (!CheckState(PROCESS_SHARDING))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in TX_SUBMISSION state");
        return false;
    }

    // [8-byte DS blocknum]
    uint64_t dsBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // Check block number
    if (!CheckWhetherDSBlockNumIsLatest(dsBlockNum + 1))
    {
        return false;
    }

    // [4-byte shard ID]
    m_myShardID
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // [Sharding structure]
    if (LoadShardingStructure(message, offset) == false)
    {
        return false;
    }

    // [Txn sharing assignments]
    LoadTxnSharingInfo(message, offset);

    POW::GetInstance().StopMining();
    /// if it is a node joining after finishing pow2, commit the state into db
    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
    {
        m_mediator.m_lookup->m_syncType = SyncType::NO_SYNC;
        AccountStore::GetInstance().MoveUpdatesToDisk();
        m_runFromLate = false;
    }
    m_fromNewProcess = false;

    if (m_mediator.m_selfKey.second == m_myShardMembersPubKeys.front())
    {
        m_isPrimary = true;
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am leader of the sharded committee");

        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << m_myShardID << "][0  ] SCLD");
    }
    else
    {
        m_isPrimary = false;

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am backup member of the sharded committee");

        LOG_STATE("[SHSTU][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "]["
                             << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                      + 1 << "] RECEIVED SHARDING STRUCTURE");

        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << m_myShardID << "][" << std::setw(3)
                             << std::left << m_consensusMyID << "] SCBK");
    }

    // Choose 4 other node to be sender of microblock to ds committee.
    // TODO: Randomly choose these nodes?
    m_isMBSender = false;
    unsigned int numOfMBSender = 5;
    if (m_myShardMembersPubKeys.size() < numOfMBSender)
    {
        numOfMBSender = m_myShardMembersPubKeys.size();
    }

    // Shard leader will not have the flag set
    for (unsigned int i = 1; i < numOfMBSender; i++)
    {
        if (m_mediator.m_selfKey.second == m_myShardMembersPubKeys.at(i))
        {
            // Selected node to be sender of its shard's micrblock
            m_isMBSender = true;
        }
    }

    m_consensusLeaderID = 0;

    // SetState(TX_SUBMISSION);

    // auto main_func = [this]() mutable -> void { SubmitTransactions(); };
    // auto expiry_func = [this]() mutable -> void {
    //   auto main_func = [this]() mutable -> void {
    //     SetState(TX_SUBMISSION_BUFFER);
    //   };
    //   auto expiry_func = [this]() mutable -> void {
    //     RunConsensusOnMicroBlock();
    //   };

    //   TimeLockedFunction tlf(SUBMIT_TX_WINDOW_EXTENDED, main_func, expiry_func, true);
    // };

    // TimeLockedFunction tlf(SUBMIT_TX_WINDOW, main_func, expiry_func, true);

    auto main_func = [this]() mutable -> void { SubmitTransactions(); };

    {
        lock_guard<mutex> g2(m_mutexNewRoundStarted);
        if (!m_newRoundStarted)
        {
            m_newRoundStarted = true;
            m_cvNewRoundStarted.notify_all();
        }
    }

    DetachedFunction(1, main_func);

    LOG_GENERAL(INFO, "Entering sleep for " << TXN_SUBMISSION << " seconds");
    this_thread::sleep_for(chrono::seconds(TXN_SUBMISSION));
    LOG_GENERAL(INFO,
                "Woken up from the sleep of " << TXN_SUBMISSION << " seconds");

    auto main_func2
        = [this]() mutable -> void { SetState(TX_SUBMISSION_BUFFER); };

    DetachedFunction(1, main_func2);

    LOG_GENERAL(INFO,
                "Using conditional variable with timeout of  "
                    << TXN_BROADCAST << " seconds. It is ok to timeout here. ");
    std::unique_lock<std::mutex> cv_lk(m_MutexCVMicroblockConsensus);
    if (cv_microblockConsensus.wait_for(cv_lk,
                                        std::chrono::seconds(TXN_BROADCAST))
        == std::cv_status::timeout)
    {
        LOG_GENERAL(INFO,
                    "Woken up from the sleep (timeout) of " << TXN_BROADCAST
                                                            << " seconds");
    }
    else
    {
        LOG_GENERAL(
            INFO,
            "I have received announcement message. Time to run consensus.");
    }

    auto main_func3 = [this]() mutable -> void { RunConsensusOnMicroBlock(); };

    DetachedFunction(1, main_func3);

#endif // IS_LOOKUP_NODE
    return true;
}
