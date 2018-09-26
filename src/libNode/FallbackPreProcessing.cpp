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

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

bool Node::FallbackValidator(
    const vector<unsigned char>& fallbackBlock,
    [[gnu::unused]] std::vector<unsigned char>& errorMsg)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::FallbackValidator not expected to be"
                    "called from LookUp node.");
        return true;
    }

    LOG_MARKER();
    lock_guard<mutex> g(m_mutexPendingFallbackBlock);

    m_pendingFallbackBlock.reset(new FallbackBlock(fallbackBlock, 0));

    // ds epoch No
    if (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
        != m_pendingFallbackBlock->GetHeader().GetFallbackDSEpochNo())
    {
        LOG_GENERAL(WARNING, "Fallback DS epoch mismatched");
        return false;
    }

    // epoch No
    if (m_mediator.m_currentEpochNum
        != m_pendingFallbackBlock->GetHeader().GetFallbackEpochNo())
    {
        LOG_GENERAL(WARNING, "Fallback epoch mismatched");
        return false;
    }

    // shard id
    if (m_myShardID != m_pendingFallbackBlock->GetHeader().GetShardId())
    {
        LOG_GENERAL(WARNING, "Fallback shard ID mismatched");
        return false;
    }

    // leader consensus id
    if (m_consensusLeaderID
        != m_pendingFallbackBlock->GetHeader().GetLeaderConsensusId())
    {
        LOG_GENERAL(WARNING, "Fallback leader consensus ID mismatched");
        return false;
    }

    // leader network info
    if (m_myShardMembers->at(m_consensusLeaderID).second
        != m_pendingFallbackBlock->GetHeader().GetLeaderNetworkInfo())
    {
        LOG_GENERAL(WARNING, "Fallback leader network info mismatched");
        return false;
    }

    // leader pub key
    if (!(m_myShardMembers->at(m_consensusLeaderID).first
          == m_pendingFallbackBlock->GetHeader().GetLeaderPubKey()))
    {
        LOG_GENERAL(WARNING, "Fallback leader pubkey mismatched");
        return false;
    }

    // fallback state
    if (!ValidateFallbackState(
            m_fallbackState,
            (NodeState)m_pendingFallbackBlock->GetHeader().GetFallbackState()))
    {
        LOG_GENERAL(WARNING,
                    "fallback state mismatched. m_fallbackState: "
                        << m_fallbackState << " Proposed: "
                        << (NodeState)m_pendingFallbackBlock->GetHeader()
                               .GetFallbackState());
        return false;
    }

    // state root hash
    if (AccountStore::GetInstance().GetStateRootHash()
        != m_pendingFallbackBlock->GetHeader().GetStateRootHash())
    {
        LOG_GENERAL(WARNING,
                    "fallback state root hash mismatched. local: "
                        << AccountStore::GetInstance().GetStateRootHash().hex()
                        << " Proposed: "
                        << m_pendingFallbackBlock->GetHeader()
                               .GetStateRootHash()
                               .hex());
        return false;
    }

    return true;
}

void Node::UpdateFallbackConsensusLeader()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::UpdateConsensusLeader not expected to be "
                    "called from LookUp node.");
        return;
    }

    // Set state to tx submission
    if (m_isPrimary == true)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am no longer the shard leader ");
        m_isPrimary = false;
    }

    m_consensusLeaderID++;
    m_consensusLeaderID = m_consensusLeaderID % COMM_SIZE;

    if (m_consensusMyID == m_consensusLeaderID)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am the new shard leader ");
        m_isPrimary = true;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "The new shard leader is m_consensusMyID "
                      << m_consensusLeaderID);
    }
}

bool Node::ValidateFallbackState(NodeState nodeState, NodeState statePropose)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::ValidateFallbackState not expected to "
                    "be called from LookUp node.");
        return true;
    }

    const std::multimap<NodeState, NodeState> STATE_CHECK_STATE
        = {{WAITING_DSBLOCK, WAITING_DSBLOCK},
           {WAITING_FINALBLOCK, WAITING_FINALBLOCK},
           {WAITING_FALLBACKBLOCK, WAITING_FALLBACKBLOCK},
           {MICROBLOCK_CONSENSUS, MICROBLOCK_CONSENSUS},
           {MICROBLOCK_CONSENSUS_PREP, MICROBLOCK_CONSENSUS_PREP},
           {MICROBLOCK_CONSENSUS_PREP, MICROBLOCK_CONSENSUS},
           {MICROBLOCK_CONSENSUS, MICROBLOCK_CONSENSUS_PREP}};

    for (auto pos = STATE_CHECK_STATE.lower_bound(nodeState);
         pos != STATE_CHECK_STATE.upper_bound(nodeState); pos++)
    {
        if (pos->second == statePropose)
        {
            return true;
        }
    }
    return false;
}

// The idea of this function is to set the last know good state of the network before fallback happens.
// This allows for the network to resume from where it left.
void Node::SetLastKnownGoodState()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::SetLastKnownGoodState not expected to "
                    "be called from LookUp node.");
        return;
    }

    switch (m_state)
    {
    case FALLBACK_CONSENSUS_PREP:
    case FALLBACK_CONSENSUS:
    case SYNC:
        break;
    default:
        m_fallbackState = (NodeState)m_state;
    }
}

void Node::ScheduleFallbackTimeout(bool started)
{
    cv_fallbackBlock.notify_all();

    std::unique_lock<std::mutex> cv_lk(m_MutexCVFallbackBlock);
    // if started, will use smaller interval
    // otherwise, using big interval
    if (!LOOKUP_NODE_MODE)
    {
        if (cv_fallbackBlock.wait_for(
                cv_lk,
                std::chrono::seconds(
                    started ? FALLBACK_INTERVAL_STARTED
                            : (FALLBACK_INTERVAL_WAITING * (m_myShardID + 1))))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Initiated fallback" << (started ? " again" : ""));

            if (started)
            {
                UpdateFallbackConsensusLeader();
            }

            auto func = [this]() -> void { RunConsensusOnFallback(); };
            DetachedFunction(1, func);
        }
    }

    if (!started)
    {
        if (cv_fallbackBlock.wait_for(
                cv_lk, std::chrono::seconds(FALLBACK_INTERVAL_WAITING))
            == std::cv_status::timeout)
        {
            if (m_state != FALLBACK_CONSENSUS_PREP
                || m_state != FALLBACK_CONSENSUS)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Ready for receiving fallback from other shard");
                cv_fallbackBlock.notify_all();
                SetState(WAITING_FALLBACKBLOCK);
            }
        }
    }
}

void Node::ComposeFallbackBlock()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::ComputeNewFallbackLeader not expected "
                    "to be called from LookUp node.");
        return;
    }

    LOG_MARKER();

    LOG_GENERAL(INFO,
                "Composing new fallback block with consensus Leader ID at "
                    << m_consensusLeaderID);

    Peer leaderNetworkInfo;
    if (m_myShardMembers->at(m_consensusLeaderID).second == Peer())
    {
        leaderNetworkInfo = m_mediator.m_selfPeer;
    }
    else
    {
        leaderNetworkInfo = m_myShardMembers->at(m_consensusLeaderID).second;
    }

    {
        lock_guard<mutex> g(m_mutexPendingFallbackBlock);
        // To-do: Handle exceptions.
        m_pendingFallbackBlock.reset(new FallbackBlock(
            FallbackBlockHeader(m_mediator.m_dsBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetBlockNum()
                                    + 1,
                                m_mediator.m_currentEpochNum, m_fallbackState,
                                AccountStore::GetInstance().GetStateRootHash(),
                                m_consensusLeaderID, leaderNetworkInfo,
                                m_myShardMembers->at(m_consensusLeaderID).first,
                                m_myShardID, get_time_as_int()),
            CoSignatures()));
    }
}

void Node::RunConsensusOnFallback()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnFallback not expected "
                    "to be called from LookUp node.");
        return;
    }

    LOG_MARKER();

    SetLastKnownGoodState();
    SetState(FALLBACK_CONSENSUS_PREP);

    LOG_GENERAL(WARNING, "Run fallback, init state delta");
    AccountStore::GetInstance().InitTemp();

    // Upon consensus object creation failure, one should not return from the function, but rather wait for fallback.
    bool ConsensusObjCreation = true;

    if (m_isPrimary)
    {
        ConsensusObjCreation = RunConsensusOnFallbackWhenLeader();
        if (!ConsensusObjCreation)
        {
            LOG_GENERAL(WARNING,
                        "Error after RunConsensusOnFallbackWhenShardLeader");
        }
    }
    else
    {
        ConsensusObjCreation = RunConsensusOnFallbackWhenBackup();
        if (!ConsensusObjCreation)
        {
            LOG_GENERAL(WARNING,
                        "Error after RunConsensusOnFallbackWhenShardBackup");
        }
    }

    if (ConsensusObjCreation)
    {
        SetState(FALLBACK_CONSENSUS);
        cv_fallbackConsensusObj.notify_all();
    }

    // schedule fallback viewchange
    auto func = [this]() -> void { ScheduleFallbackTimeout(true /*started*/); };
    DetachedFunction(1, func);
}

bool Node::RunConsensusOnFallbackWhenLeader()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::"
                    "RunConsensusOnFallbackWhenLeader not expected "
                    "to be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the fallback leader node. Announcing to the rest.");

    ComposeFallbackBlock();

    // Create new consensus object
    // Dummy values for now
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, *m_myShardMembers,
        static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(FALLBACKCONSENSUS),
        std::function<bool(const vector<unsigned char>&, unsigned int,
                           const Peer&)>(),
        std::function<bool(map<unsigned int, vector<unsigned char>>)>()));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Error: Unable to create consensus leader object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    vector<unsigned char> m;
    {
        lock_guard<mutex> g(m_mutexPendingFallbackBlock);
        m_pendingFallbackBlock->Serialize(m, 0);
    }

    std::this_thread::sleep_for(std::chrono::seconds(FALLBACK_EXTRA_TIME));
    cl->StartConsensus(m, FallbackBlockHeader::SIZE);

    return true;
}

bool Node::RunConsensusOnFallbackWhenBackup()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::RunConsensusOnFallbackWhenBackup not "
                    "expected to be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am a fallback backup node. Waiting for Fallback announcement.");

    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return FallbackValidator(message, errorMsg);
    };

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first, *m_myShardMembers,
        static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(FALLBACKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Error: Unable to create consensus backup object");
        return false;
    }

    return true;
}