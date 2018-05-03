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

bool DirectoryService::ViewChange()
{
    /**
    if (m_mediator.m_DSCommitteeNetworkInfo.at(1) == candidiateLeader)
    {
        // View change

        // Kick current leader to the back of the queue, waiting to be eject at
        // the next ds epoch
        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();

        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();

        // New leader for consensus
        m_consensusMyID--;
        m_viewChangeCounter++;

        switch (m_state)
        {
        case DSBLOCK_CONSENSUS:
            SetState(DSBLOCK_CONSENSUS_PREP);
        case DSBLOCK_CONSENSUS_PREP:
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                         "Re-running dsblock consensus (backup)");
            RunConsensusOnDSBlockWhenDSBackup();
            SetState(DSBLOCK_CONSENSUS);
            break;
        case SHARDING_CONSENSUS:
            SetState(SHARDING_CONSENSUS_PREP);
        case SHARDING_CONSENSUS_PREP:
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                         "Re-running sharding consensus (backup)");
            RunConsensusOnShardingWhenDSBackup();
            SetState(SHARDING_CONSENSUS);
            break;
        case FINALBLOCK_CONSENSUS:
            SetState(FINALBLOCK_CONSENSUS_PREP);
        case FINALBLOCK_CONSENSUS_PREP:
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                         "Re-running finalblock consensus (backup)");
            RunConsensusOnFinalBlockWhenDSBackup();
            SetState(FINALBLOCK_CONSENSUS);
            break;
        default:
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                         "illegal view change state (backup)");
        }
        return false;
    }
    **/
    return true;
}

bool DirectoryService::ProcessViewChangeConsensus(
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

    /**
    // if (m_state != SHARDING_CONSENSUS)
    if (!CheckState(PROCESS_SHARDINGCONSENSUS))
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Ignoring consensus message");
        return false;
    }
    **/

    bool result = m_consensusObject->ProcessMessage(message, offset, from);
    ConsensusCommon::State state = m_consensusObject->GetState();
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                 "Consensus state = " << state);

    if (state == ConsensusCommon::State::DONE)
    {
        ViewChange();
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "View change consensus is DONE!!!");
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Oops, no consensus reached - what to do now???");
        // throw exception();
        // TODO: no consensus reached
        return false;
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}
