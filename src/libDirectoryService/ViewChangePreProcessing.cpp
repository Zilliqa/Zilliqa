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

    VCBlock proposedVCBlock(vcBlock, 0);
    uint32_t offsetToNewLeader = 1;

    if (m_mediator.m_DSCommitteeNetworkInfo.at(offsetToNewLeader)
        != proposedVCBlock.GetHeader().GetCandidateLeaderNetworkInfo())
    {
        return false;
    }

    if (!(m_mediator.m_DSCommitteePubKeys.at(offsetToNewLeader)
          == proposedVCBlock.GetHeader().GetCandidateLeaderPubKey()))
    {
        return false;
    }

    if (m_viewChangestate != proposedVCBlock.GetHeader().GetViewChangeState())
    {
        return false;
    }

    if (m_viewChangeCounter
        != proposedVCBlock.GetHeader().GetViewChangeCounter())
    {
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
    unsigned int newCandidateLeader
        = 1; // TODO: To be change to a random node using VRF
    if (m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeader)
        == m_mediator.m_selfPeer)
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

void DirectoryService::ComputeNewCandidateLeader(
    vector<unsigned char>& newCandidateLeader)
{
    LOG_MARKER();

    // [candidate leader index] [candidate leader ip] [candidate leader port] [candidate leader pubkey] [Cur State]

    unsigned int newCandidateLeaderIndex = 1;
    unsigned int curr_offset = 0;

    Serializable::SetNumber<unsigned int>(newCandidateLeader, curr_offset,
                                          newCandidateLeaderIndex,
                                          sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    curr_offset
        += m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeaderIndex)
               .Serialize(newCandidateLeader, curr_offset);
    curr_offset += m_mediator.m_DSCommitteePubKeys.at(newCandidateLeaderIndex)
                       .Serialize(newCandidateLeader, curr_offset);

    Serializable::SetNumber<unsigned int>(newCandidateLeader, curr_offset,
                                          m_state, sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    // Assemble VC block header

    LOG_GENERAL(INFO,
                "Composing new vc block with vc count at "
                    << m_viewChangeCounter);

    VCBlockHeader newHeader(
        m_mediator.m_dsBlockChain.GetBlockCount(), m_mediator.m_currentEpochNum,
        m_viewChangestate, newCandidateLeaderIndex,
        m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeaderIndex),
        m_mediator.m_DSCommitteePubKeys.at(newCandidateLeaderIndex),
        m_viewChangeCounter, get_time_as_int());

    array<unsigned char, BLOCK_SIG_SIZE> newSig{};
    {
        lock_guard<mutex> g(m_mutexPendingVCBlock);
        // To-do: Handle exceptions.
        m_pendingVCBlock.reset(new VCBlock(newHeader, newSig));
    }

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "New VCBlock created with candidate leader "
            << m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeaderIndex)
                   .GetPrintableIPAddress()
            << ":"
            << m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeaderIndex)
                   .m_listenPortHost);
}

bool DirectoryService::RunConsensusOnViewChangeWhenCandidateLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the candidate leader DS node. Announcing to the rest.");

    vector<unsigned char> newCandidateLeader;
    ComputeNewCandidateLeader(newCandidateLeader);

    // Create new consensus object
    // Dummy values for now
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    // kill first ds leader
    // if (m_consensusMyID == 0 && temp_todie)
    // {
    //    LOG_MESSAGE("I am killing myself to test view change");
    //    throw exception();
    // }
    uint32_t consensusID = m_viewChangeCounter;
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

    cl->StartConsensus(newCandidateLeader, VCBlockHeader::SIZE);

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
