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

#ifndef IS_LOOKUP_NODE
bool DirectoryService::ViewChangeValidator(const vector<unsigned char>& vcBlock,
                                           std::vector<unsigned char>& errorMsg)
{
    LOG_MARKER();
    lock_guard<mutex> g(m_mutexPendingVCBlock);

    m_pendingVCBlock.reset(new VCBlock(vcBlock, 0));
    uint32_t offsetToNewLeader = 1;

    if (m_mediator.m_DSCommitteeNetworkInfo.at(offsetToNewLeader)
        != m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo())
    {
        LOG_GENERAL(WARNING, "Candidate network info mismatched");
        return false;
    }

    if (!(m_mediator.m_DSCommitteePubKeys.at(offsetToNewLeader)
          == m_pendingVCBlock->GetHeader().GetCandidateLeaderPubKey()))
    {
        LOG_GENERAL(WARNING, "Candidate pubkey mismatched");
        return false;
    }

    if (m_viewChangestate != m_pendingVCBlock->GetHeader().GetViewChangeState())
    {
        LOG_GENERAL(WARNING, "View change state mismatched");
        return false;
    }

    if (m_viewChangeCounter
        != m_pendingVCBlock->GetHeader().GetViewChangeCounter())
    {
        LOG_GENERAL(WARNING, "View change counter mismatched");
        return false;
    }
    return true;
}

void DirectoryService::RunConsensusOnViewChange()
{
    LOG_MARKER();

    m_viewChangestate = (DirState)m_state;
    SetState(VIEWCHANGE_CONSENSUS_PREP); //change

    m_viewChangeCounter++;

    for (unsigned i = 0; i < m_mediator.m_DSCommitteeNetworkInfo.size(); i++)
    {
        LOG_GENERAL(INFO, m_mediator.m_DSCommitteeNetworkInfo.at(i));
    }

    unsigned int newCandidateLeader
        = 1; // TODO: To be change to a random node using VRF

    // TODO
    // We compare with empty peer is due to the fact that DSCommittee for yourself is 0.0.0.0 with port 0.
    if (m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeader) == Peer())
    {
        if (!RunConsensusOnViewChangeWhenCandidateLeader())
        {
            LOG_GENERAL(
                WARNING,
                "Throwing exception after RunConsensusOnDSBlockWhenDSPrimary");
            return;
        }
    }
    else
    {
        if (!RunConsensusOnViewChangeWhenNotCandidateLeader())
        {
            LOG_GENERAL(WARNING,
                        "Throwing exception after "
                        "RunConsensusOnViewChangeWhenNotCandidateLeader");
            return;
        }
    }

    SetState(VIEWCHANGE_CONSENSUS);
    cv_ViewChangeConsensusObj.notify_all();

    auto func = [this]() -> void { ScheduleViewChangeTimeout(); };
    DetachedFunction(1, func);
}

void DirectoryService::ScheduleViewChangeTimeout()
{
    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeVCBlock);
    if (cv_ViewChangeVCBlock.wait_for(cv_lk,
                                      std::chrono::seconds(VIEWCHANGE_TIME))
        == std::cv_status::timeout)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Initiated view change again. ");

        auto func = [this]() -> void { RunConsensusOnViewChange(); };
        DetachedFunction(1, func);
    }
}

void DirectoryService::ComputeNewCandidateLeader()
{
    LOG_MARKER();

    // Assemble VC block header

    LOG_GENERAL(INFO,
                "Composing new vc block with vc count at "
                    << m_viewChangeCounter);
    uint32_t newCandidateLeaderIndex = 1;

    Peer newLeaderNetworkInfo;
    if (m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeaderIndex)
        == Peer())
    {
        // I am the leader but in the Peer store, it is put as 0.0.0.0 with port 0
        newLeaderNetworkInfo = m_mediator.m_selfPeer;
    }
    else
    {
        newLeaderNetworkInfo
            = m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeaderIndex);
    }

    {
        lock_guard<mutex> g(m_mutexPendingVCBlock);
        // To-do: Handle exceptions.
        m_pendingVCBlock.reset(new VCBlock(
            VCBlockHeader(
                (uint64_t)m_mediator.m_dsBlockChain.GetBlockCount(),
                (uint64_t)m_mediator.m_currentEpochNum, m_viewChangestate,
                newCandidateLeaderIndex, newLeaderNetworkInfo,
                m_mediator.m_DSCommitteePubKeys.at(newCandidateLeaderIndex),
                m_viewChangeCounter, get_time_as_int()),
            CoSignatures()));
    }
}

bool DirectoryService::RunConsensusOnViewChangeWhenCandidateLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the candidate leader DS node. Announcing to the rest.");

    ComputeNewCandidateLeader();

    uint32_t consensusID = m_viewChangeCounter;
    // Create new consensus object
    // Dummy values for now
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    m_consensusObject.reset(new ConsensusLeader(
        consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommitteePubKeys,
        m_mediator.m_DSCommitteeNetworkInfo,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(VIEWCHANGECONSENSUS),
        std::function<bool(const vector<unsigned char>&, unsigned int,
                           const Peer&)>(),
        std::function<bool(map<unsigned int, vector<unsigned char>>)>()));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Error: Unable to create consensus object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    vector<unsigned char> m;
    {
        lock_guard<mutex> g(m_mutexPendingVCBlock);
        m_pendingVCBlock->Serialize(m, 0);
    }

    std::this_thread::sleep_for(std::chrono::seconds(VIEWCHANGE_EXTRA_TIME));
    cl->StartConsensus(m, VCBlockHeader::SIZE);

    return true;
}

bool DirectoryService::RunConsensusOnViewChangeWhenNotCandidateLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a backup DS node (after view change). Waiting for View "
              "Change announcement.");

    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return ViewChangeValidator(message, errorMsg);
    };

    uint32_t consensusID = m_viewChangeCounter;
    uint32_t offsetToNewLeader = 1;
    m_consensusObject.reset(new ConsensusBackup(
        consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID + offsetToNewLeader, m_mediator.m_selfKey.first,
        m_mediator.m_DSCommitteePubKeys, m_mediator.m_DSCommitteeNetworkInfo,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(VIEWCHANGECONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Error: Unable to create consensus object");
        return false;
    }

    return true;
}
#endif // IS_LOOKUP_NODE
