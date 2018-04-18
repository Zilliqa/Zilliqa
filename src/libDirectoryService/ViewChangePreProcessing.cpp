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

bool DirectoryService::ViewChangeValidator(
    const vector<unsigned char>& newCandidateLeader,
    std::vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // Check new leader index
    unsigned int curr_offset = 0;
    unsigned int expectedCandidateLeaderIndex = 1;

    // new leader index
    unsigned int candidateLeaderIndex = Serializable::GetNumber<uint32_t>(
        newCandidateLeader, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(unsigned int);

    if (candidateLeaderIndex != expectedCandidateLeaderIndex)
    {
        LOG_MESSAGE("Error: Wrong candidate leader index");
        return false;
    }

    // Check new leader ip and port
    Peer candidateLeaderIPAndPort(newCandidateLeader, curr_offset);
    curr_offset += IP_SIZE + PORT_SIZE;

    if (m_mediator.m_DSCommitteeNetworkInfo.at(candidateLeaderIndex)
        != candidateLeaderIPAndPort)
    {
        LOG_MESSAGE("Error: Wrong candidate leader ip and port");
        return false;
    }

    // Check new leader pub key
    PubKey candidateLeaderPubKey(newCandidateLeader, curr_offset);
    curr_offset += PUB_KEY_SIZE;

    if (m_mediator.m_DSCommitteePubKeys.at(candidateLeaderIndex)
        != candidateLeaderPubKey)
    {
        LOG_MESSAGE("Error: Wrong candidate leader pub key");
        return false;
    }

    // Check whether view change is in permitted state or not
    unsigned int viewChangeDSState;
    viewChangeDSState = Serializable::GetNumber<unsigned int>(
        message, curr_offset, sizeof(unsigned int));
    curr_offset += sizeof(unsigned int);

    switch (viewChangeDSState)
    {
    case DSBLOCK_CONSENSUS_PREP:
    case DSBLOCK_CONSENSUS:
    case SHARDING_CONSENSUS_PREP:
    case SHARDING_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
    case FINALBLOCK_CONSENSUS:
        break;
    default:
        return false;
    }

    return true;
}

void DirectoryService::RunConsensusOnViewChange()
{
    LOG_MARKER();
    SetState(VIEWCHANGE_CONSENSUS_PREP); //change
    unique_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);

    unsigned int newCandidateLeader = 1; // To be change to a random node
    if (m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeader)
        == m_mediator.m_selfPeer)
    {
        if (!RunConsensusOnViewChangeWhenCandidateLeader())
        {
            LOG_MESSAGE(
                "Throwing exception after RunConsensusOnDSBlockWhenDSPrimary");
            // throw exception();
            return;
        }
    }
    else
    {
        if (!RunConsensusOnViewChangeWhenNotCandidateLeader())
        {
            LOG_MESSAGE("Throwing exception after "
                        "RunConsensusOnViewChangeWhenNotCandidateLeader");
            // throw exception();
            return;
        }
    }

    SetState(VIEWCHANGE_CONSENSUS);

    /** reserve for another view change
    if (m_mode != PRIMARY_DS)
    {
        std::unique_lock<std::mutex> cv_lk(m_mutexRecoveryDSBlockConsensus);
        if (cv_RecoveryDSBlockConsensus.wait_for(
                cv_lk, std::chrono::seconds(VIEWCHANGE_TIME))
            == std::cv_status::timeout)
        {
            //View change.
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                         "Initiated view change again. ");
            RunConsensusOnInitViewChange();
        }
    }
    **/
}

void DirectoryService::ComputeNewCandidateLeader(
    vector<unsigned char>& newCandidateLeader) const
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
}

bool DirectoryService::RunConsensusOnViewChangeWhenCandidateLeader()
{
    LOG_MARKER();

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                 "I am the candidate leader DS node. Proposing to the rest.");

    vector<unsigned char> newCandidateLeader;
    ComputeNewCandidateLeader(newCandidateLeader);

    // Create new consensus object
    // Dummy values for now
    uint32_t consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    // kill first ds leader
    // if (m_consensusMyID == 0 && temp_todie)
    // {
    //    LOG_MESSAGE("I am killing myself to test view change");
    //    throw exception();
    // }

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
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Error: Unable to create consensus object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    cl->StartConsensus(newCandidateLeader);

    return true;
}

bool DirectoryService::RunConsensusOnViewChangeWhenNotCandidateLeader()
{
    LOG_MARKER();

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                 "I am a backup DS node (after view change). Waiting for View "
                 "Change announcement.");

    // Dummy values for now
    uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return ViewChangeValidator(message, errorMsg);
    };

    m_consensusObject.reset(new ConsensusBackup(
        consensusID, m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
        m_mediator.m_selfKey.first, m_mediator.m_DSCommitteePubKeys,
        m_mediator.m_DSCommitteeNetworkInfo,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(VIEWCHANGECONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Error: Unable to create consensus object");
        return false;
    }

    return true;
}
