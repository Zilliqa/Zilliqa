/**
* Copyright (c) 2017 Zilliqa 
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

#include <thread>
#include <chrono>
#include <array>
#include <functional>
#include <boost/multiprecision/cpp_int.hpp>

#include "Node.h"
#include "common/Serializable.h"
#include "common/Messages.h"
#include "common/Constants.h"
#include "depends/common/RLP.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "depends/libDatabase/MemoryDB.h"
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

bool Node::ReadVariablesFromShardingMessage(const vector<unsigned char> & message, unsigned int cur_offset)
{
    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), cur_offset, sizeof(uint256_t) + sizeof(uint32_t) +
            sizeof(uint32_t) + sizeof(uint32_t)))
    {
        return false;
    }

    // 32-byte block number
    uint256_t dsBlockNum = Serializable::GetNumber<uint256_t>(message, cur_offset, sizeof(uint256_t));
    cur_offset += sizeof(uint256_t);

    // Check block number
    if (!CheckWhetherDSBlockNumIsLatest(dsBlockNum + 1))
    {
        return false;
    }

    // 4-byte shard ID
    m_myShardID = Serializable::GetNumber<uint32_t>(message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // 4-byte number of shards
    m_numShards = Serializable::GetNumber<uint32_t>(message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // 4-byte committee size
    uint32_t comm_size = Serializable::GetNumber<uint32_t>(message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    if (IsMessageSizeInappropriate(message.size(), cur_offset, (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE) * comm_size))
    {
        return false;
    }

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Committee size = " << comm_size);
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Members:");
    
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

        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), " PubKey: " << DataConversion::SerializableToHexStr(m_myShardMembersPubKeys.back()) <<
            " IP: " << m_myShardMembersNetworkInfo.back().GetPrintableIPAddress() <<
            " Port: " << m_myShardMembersNetworkInfo.back().m_listenPortHost);
    }

    return true;
}

bool Node::ProcessSharding(const vector<unsigned char> & message, unsigned int offset, 
                           const Peer & from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [32-byte DS blocknum] [4-byte shard ID] [4-byte committee size] [33-byte public key]
    // [16-byte ip] [4-byte port] ... (all nodes; first entry is leader)
    LOG_MARKER();

    POW::GetInstance().StopMining(); // TODO

    m_mediator.m_isConnectedToNetwork = true;

    // if (m_state != TX_SUBMISSION)
    if (!CheckState(PROCESS_SHARDING))
    {
        // LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Valid SHARDING already received. Ignoring redundant SHARDING message.");
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Error: Not in TX_SUBMISSION state");
        return false;
    }

    if(!ReadVariablesFromShardingMessage(message, offset))
    {
        return false;
    }

    if (m_mediator.m_selfKey.second == m_myShardMembersPubKeys.front())
    {
        m_isPrimary = true;
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "I am primary of the sharded committee");

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << std::setw(15) << std::left << m_mediator.m_selfPeer.GetPrintableIPAddress() << "][" << m_myShardID << "][0  ] SCLD");
#endif
    }
    else
    {
        m_isPrimary = false;
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "I am backup member of the sharded committee");

#ifdef STAT_TEST
        LOG_STATE("[IDENT][" << std::setw(15) << std::left << m_mediator.m_selfPeer.GetPrintableIPAddress() << "][" << m_myShardID << "][" << std::setw(3) << std::left << m_consensusMyID << "] SCBK");
#endif // STAT_TEST
    }

    m_consensusLeaderID = 0;

    // SetState(TX_SUBMISSION);

    auto main_func = [this]() mutable -> void { SubmitTransactions(); };
    auto expiry_func = [this]() mutable -> void { RunConsensusOnMicroBlock(); };

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Waiting " << SUBMIT_TX_WINDOW<< " seconds, accepting Tx submissions for epoch " << m_mediator.m_currentEpochNum);
    TimeLockedFunction tlf(SUBMIT_TX_WINDOW, main_func, expiry_func, true);
#endif // IS_LOOKUP_NODE
    return true;
}