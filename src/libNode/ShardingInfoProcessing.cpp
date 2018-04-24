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

bool Node::ReadVariablesFromShardingMessage(
    const vector<unsigned char>& message, unsigned int cur_offset)
{
    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                   sizeof(unsigned int) + sizeof(uint256_t)
                                       + sizeof(uint32_t) + sizeof(uint32_t)
                                       + sizeof(uint32_t)))
    {
        return false;
    }

    // view change counter
    unsigned int viewChangeCounter = Serializable::GetNumber<unsigned int>(
        message, cur_offset, sizeof(unsigned int));
    cur_offset += sizeof(unsigned int);

    for (unsigned int i = 0; i < viewChangeCounter; i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();
        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();
    }

    // 32-byte block number
    uint256_t dsBlockNum = Serializable::GetNumber<uint256_t>(
        message, cur_offset, sizeof(uint256_t));
    cur_offset += sizeof(uint256_t);

    // Check block number
    if (!CheckWhetherDSBlockNumIsLatest(dsBlockNum + 1))
    {
        return false;
    }

    // 4-byte shard ID
    m_myShardID = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                    sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // 4-byte number of shards
    m_numShards = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                    sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // 4-byte committee size
    uint32_t comm_size = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                           sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                   (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE)
                                       * comm_size))
    {
        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Committee size = " << comm_size);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Members:");

    m_myShardMembersPubKeys.clear();
    m_myShardMembersNetworkInfo.clear();

    // All nodes; first entry is leader
    for (uint32_t i = 0; i < comm_size; i++)
    {
        m_myShardMembersPubKeys.push_back(PubKey(message, cur_offset));
        cur_offset += PUB_KEY_SIZE;

        m_myShardMembersNetworkInfo.push_back(Peer(message, cur_offset));
        cur_offset += IP_SIZE + PORT_SIZE;

        // Zero out my IP to avoid sending to myself
        if (m_mediator.m_selfPeer == m_myShardMembersNetworkInfo.back())
        {
            m_consensusMyID = i; // Set my ID
            //m_myShardMembersNetworkInfo.back().m_ipAddress = 0;
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
    }

    return true;
}

bool Node::ProcessSharding(const vector<unsigned char>& message,
                           unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [32-byte DS blocknum] [4-byte shard ID] [4-byte committee size] [33-byte public key]
    // [16-byte ip] [4-byte port] ... (all nodes; first entry is leader)
    LOG_MARKER();

    POW::GetInstance().StopMining();

    /// if it is a node joining after finishing pow2, commit the state into db
    if (!m_mediator.m_isConnectedToNetwork)
    {
        m_mediator.m_isConnectedToNetwork = true;
        AccountStore::GetInstance().MoveUpdatesToDisk();
        m_isNewNode = false;
        m_runFromLate = false;
    }

    // if (m_state != TX_SUBMISSION)
    if (!CheckState(PROCESS_SHARDING))
    {
        // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Valid SHARDING already received. Ignoring redundant SHARDING message.");
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in TX_SUBMISSION state");
        return false;
    }

    if (!ReadVariablesFromShardingMessage(message, offset))
    {
        return false;
    }

    if (m_mediator.m_selfKey.second == m_myShardMembersPubKeys.front())
    {
        m_isPrimary = true;
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am primary of the sharded committee");

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << m_myShardID << "][0  ] SCLD");
#endif
    }
    else
    {
        m_isPrimary = false;
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am backup member of the sharded committee");

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << m_myShardID << "][" << std::setw(3)
                             << std::left << m_consensusMyID << "] SCBK");
#endif // STAT_TEST
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
    //     unique_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);
    //     SetState(TX_SUBMISSION_BUFFER);
    //   };
    //   auto expiry_func = [this]() mutable -> void {
    //     RunConsensusOnMicroBlock();
    //   };

    //   TimeLockedFunction tlf(SUBMIT_TX_WINDOW_EXTENDED, main_func, expiry_func, true);
    // };

    // TimeLockedFunction tlf(SUBMIT_TX_WINDOW, main_func, expiry_func, true);

    auto main_func = [this]() mutable -> void { SubmitTransactions(); };
    DetachedFunction(1, main_func);

    LOG_GENERAL(INFO, "I am going to sleep for 15 seconds");
    this_thread::sleep_for(chrono::seconds(15));
    LOG_GENERAL(INFO, "I have woken up from the sleep of 15 seconds");

    auto main_func2 = [this]() mutable -> void {
        // unique_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);
        SetState(TX_SUBMISSION_BUFFER);
    };
    DetachedFunction(1, main_func2);

    LOG_GENERAL(INFO, "I am going to sleep for 30 seconds");
    this_thread::sleep_for(chrono::seconds(30));
    LOG_GENERAL(INFO, "I have woken up from the sleep of 30 seconds");

    auto main_func3 = [this]() mutable -> void { RunConsensusOnMicroBlock(); };
    DetachedFunction(1, main_func3);

#endif // IS_LOOKUP_NODE
    return true;
}
