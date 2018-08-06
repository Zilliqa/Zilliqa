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
bool DirectoryService::RunConsensusOnShardingWhenDSPrimary()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the leader DS node. Creating sharding structure.");

    // Send just a dummy byte now
    vector<unsigned char> sharding_structure = {0};

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

    LOG_STATE(
        "[SHCON]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "] BGIN");

    cl->StartConsensus(sharding_structure, sharding_structure.size());

    return true;
}

bool DirectoryService::ShardingValidator(
    [[gnu::unused]] const vector<unsigned char>& sharding_structure,
    [[gnu::unused]] std::vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // Do nothing now

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
