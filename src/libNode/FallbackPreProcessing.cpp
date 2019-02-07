/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/TimestampVerifier.h"

using namespace std;

bool Node::FallbackValidator(const bytes& message, unsigned int offset,
                             [[gnu::unused]] bytes& errorMsg,
                             const uint32_t consensusID,
                             const uint64_t blockNumber, const bytes& blockHash,
                             const uint16_t leaderID, const PubKey& leaderKey,
                             bytes& messageToCosign) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::FallbackValidator not expected to be"
                "called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  lock_guard<mutex> g(m_mutexPendingFallbackBlock);

  m_pendingFallbackBlock.reset(new FallbackBlock);

  if (!Messenger::GetNodeFallbackBlockAnnouncement(
          message, offset, consensusID, blockNumber, blockHash, leaderID,
          leaderKey, *m_pendingFallbackBlock, messageToCosign)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetNodeFallbackBlockAnnouncement failed.");
    return false;
  }

  if (m_pendingFallbackBlock->GetHeader().GetVersion() !=
      FALLBACKBLOCK_VERSION) {
    LOG_CHECK_FAIL("FallbackBlock version",
                   m_pendingFallbackBlock->GetHeader().GetVersion(),
                   FALLBACKBLOCK_VERSION);
    return false;
  }

  BlockHash temp_blockHash = m_pendingFallbackBlock->GetHeader().GetMyHash();
  if (temp_blockHash != m_pendingFallbackBlock->GetBlockHash()) {
    LOG_GENERAL(WARNING,
                "Block Hash in Newly received FB Block doesn't match. "
                "Calculated: "
                    << temp_blockHash << " Received: "
                    << m_pendingFallbackBlock->GetBlockHash().hex());
    return false;
  }

  // Check timestamp
  if (!VerifyTimestamp(m_pendingFallbackBlock->GetTimestamp(),
                       CONSENSUS_OBJECT_TIMEOUT)) {
    return false;
  }

  if (!m_mediator.CheckWhetherBlockIsLatest(
          m_pendingFallbackBlock->GetHeader().GetFallbackDSEpochNo(),
          m_pendingFallbackBlock->GetHeader().GetFallbackEpochNo())) {
    LOG_GENERAL(WARNING, "FallbackValidator CheckWhetherBlockIsLatest failed");
    return false;
  }

  // shard id
  if (m_myshardId != m_pendingFallbackBlock->GetHeader().GetShardId()) {
    LOG_GENERAL(WARNING,
                "Fallback shard ID mismatched"
                    << endl
                    << "expected: " << m_myshardId << endl
                    << "received: "
                    << m_pendingFallbackBlock->GetHeader().GetShardId());
    return false;
  }

  // verify the shard committee hash
  CommitteeHash committeeHash;
  if (!Messenger::GetShardHash(m_mediator.m_ds->m_shards.at(m_myshardId),
                               committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetShardHash failed.");
    return false;
  }
  if (committeeHash != m_pendingFallbackBlock->GetHeader().GetCommitteeHash()) {
    LOG_GENERAL(WARNING,
                "Fallback committee hash mismatched"
                    << endl
                    << "expected: " << committeeHash << endl
                    << "received: "
                    << m_pendingFallbackBlock->GetHeader().GetCommitteeHash());
    return false;
  }

  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      m_mediator.m_blocklinkchain.GetLatestBlockLink());
  if (prevHash != m_pendingFallbackBlock->GetHeader().GetPrevHash()) {
    LOG_GENERAL(WARNING,
                "Prev Block hash in newly received Fallback Block doesn't "
                "match. Calculated "
                    << prevHash << " Received"
                    << m_pendingFallbackBlock->GetHeader().GetPrevHash());
    return false;
  }

  // leader consensus id
  if (m_consensusLeaderID !=
      m_pendingFallbackBlock->GetHeader().GetLeaderConsensusId()) {
    LOG_GENERAL(
        WARNING,
        "Fallback leader consensus ID mismatched"
            << endl
            << "expected: " << m_consensusLeaderID << endl
            << "received: "
            << m_pendingFallbackBlock->GetHeader().GetLeaderConsensusId());
    return false;
  }

  // leader network info
  {
    lock_guard<mutex> g(m_mutexShardMember);
    if (m_myShardMembers->at(m_consensusLeaderID).second !=
        m_pendingFallbackBlock->GetHeader().GetLeaderNetworkInfo()) {
      LOG_GENERAL(
          WARNING,
          "Fallback leader network info mismatched"
              << endl
              << "expected: "
              << m_myShardMembers->at(m_consensusLeaderID).second << endl
              << "received: "
              << m_pendingFallbackBlock->GetHeader().GetLeaderNetworkInfo());
      return false;
    }
    // leader pub key
    if (!(m_myShardMembers->at(m_consensusLeaderID).first ==
          m_pendingFallbackBlock->GetHeader().GetLeaderPubKey())) {
      LOG_GENERAL(WARNING,
                  "Fallback leader pubkey mismatched"
                      << endl
                      << "expected: "
                      << m_myShardMembers->at(m_consensusLeaderID).first << endl
                      << "received: "
                      << m_pendingFallbackBlock->GetHeader().GetLeaderPubKey());
      return false;
    }
  }

  // fallback state
  if (!ValidateFallbackState(
          m_fallbackState,
          (NodeState)m_pendingFallbackBlock->GetHeader().GetFallbackState())) {
    LOG_GENERAL(WARNING, "fallback state mismatched. m_fallbackState: "
                             << m_fallbackState << " Proposed: "
                             << (NodeState)m_pendingFallbackBlock->GetHeader()
                                    .GetFallbackState());
    return false;
  }

  // state root hash
  if (AccountStore::GetInstance().GetStateRootHash() !=
      m_pendingFallbackBlock->GetHeader().GetStateRootHash()) {
    LOG_GENERAL(
        WARNING,
        "fallback state root hash mismatched"
            << endl
            << "expected: "
            << AccountStore::GetInstance().GetStateRootHash().hex() << endl
            << " received: "
            << m_pendingFallbackBlock->GetHeader().GetStateRootHash().hex());
    return false;
  }

  return true;
}

void Node::UpdateFallbackConsensusLeader() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::UpdateConsensusLeader not expected to be "
                "called from LookUp node.");
    return;
  }

  // Set state to tx submission
  if (m_isPrimary) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I am no longer the shard leader ");
    m_isPrimary = false;
  }

  m_consensusLeaderID++;
  {
    lock_guard<mutex> g(m_mutexShardMember);
    m_consensusLeaderID = m_consensusLeaderID % m_myShardMembers->size();
  }

  if (m_consensusMyID == m_consensusLeaderID) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "I am the new shard leader ");
    m_isPrimary = true;
  } else {
    LOG_EPOCH(
        INFO, m_mediator.m_currentEpochNum,
        "The new shard leader is m_consensusMyID " << m_consensusLeaderID);
  }
}

bool Node::ValidateFallbackState(NodeState nodeState, NodeState statePropose) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ValidateFallbackState not expected to "
                "be called from LookUp node.");
    return true;
  }

  const std::multimap<NodeState, NodeState> STATE_CHECK_STATE = {
      {WAITING_DSBLOCK, WAITING_DSBLOCK},
      {WAITING_FINALBLOCK, WAITING_FINALBLOCK},
      {WAITING_FALLBACKBLOCK, WAITING_FALLBACKBLOCK},
      {MICROBLOCK_CONSENSUS, MICROBLOCK_CONSENSUS},
      {MICROBLOCK_CONSENSUS_PREP, MICROBLOCK_CONSENSUS_PREP},
      {MICROBLOCK_CONSENSUS_PREP, MICROBLOCK_CONSENSUS},
      {MICROBLOCK_CONSENSUS, MICROBLOCK_CONSENSUS_PREP}};

  for (auto pos = STATE_CHECK_STATE.lower_bound(nodeState);
       pos != STATE_CHECK_STATE.upper_bound(nodeState); pos++) {
    if (pos->second == statePropose) {
      return true;
    }
  }
  return false;
}

// The idea of this function is to set the last know good state of the network
// before fallback happens. This allows for the network to resume from where it
// left.
void Node::SetLastKnownGoodState() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SetLastKnownGoodState not expected to "
                "be called from LookUp node.");
    return;
  }

  switch (m_state) {
    case FALLBACK_CONSENSUS_PREP:
    case FALLBACK_CONSENSUS:
    case SYNC:
      break;
    default:
      m_fallbackState = (NodeState)m_state;
  }
}

void Node::FallbackTimerLaunch() {
  if (m_fallbackTimerLaunched) {
    return;
  }

  if (!ENABLE_FALLBACK) {
    LOG_GENERAL(INFO, "Fallback is currently disabled");
    return;
  }

  LOG_MARKER();

  if (FALLBACK_INTERVAL_STARTED < FALLBACK_CHECK_INTERVAL ||
      FALLBACK_INTERVAL_WAITING < FALLBACK_CHECK_INTERVAL) {
    LOG_GENERAL(WARNING,
                "FATAL The configured fallback checking interval must be "
                "smaller than the timeout value.");
    return;
  }

  m_runFallback = true;
  m_fallbackTimer = 0;
  m_fallbackStarted = false;

  auto func = [this]() -> void {
    while (m_runFallback) {
      this_thread::sleep_for(chrono::seconds(FALLBACK_CHECK_INTERVAL));

      if (m_mediator.m_ds->m_mode != DirectoryService::IDLE) {
        m_fallbackTimerLaunched = false;
        return;
      }

      lock_guard<mutex> g(m_mutexFallbackTimer);

      if (m_fallbackStarted) {
        if (LOOKUP_NODE_MODE) {
          LOG_GENERAL(WARNING,
                      "Node::FallbackTimerLaunch when started is "
                      "true not expected to be called from "
                      "LookUp node.");
          return;
        }

        if (m_fallbackTimer >= FALLBACK_INTERVAL_STARTED) {
          UpdateFallbackConsensusLeader();

          auto func = [this]() -> void { RunConsensusOnFallback(); };
          DetachedFunction(1, func);

          m_fallbackTimer = 0;
        }
      } else {
        bool runConsensus = false;

        if (!LOOKUP_NODE_MODE) {
          if (m_fallbackTimer >=
              (FALLBACK_INTERVAL_WAITING * (m_myshardId + 1))) {
            auto func = [this]() -> void { RunConsensusOnFallback(); };
            DetachedFunction(1, func);
            m_fallbackStarted = true;
            runConsensus = true;
            m_fallbackTimer = 0;
            m_justDidFallback = true;
          }
        }

        if (m_fallbackTimer >= FALLBACK_INTERVAL_WAITING &&
            m_state != WAITING_FALLBACKBLOCK &&
            m_state != FALLBACK_CONSENSUS_PREP &&
            m_state != FALLBACK_CONSENSUS && !runConsensus) {
          SetState(WAITING_FALLBACKBLOCK);
          m_justDidFallback = true;
          cv_fallbackBlock.notify_all();
        }
      }

      m_fallbackTimer += FALLBACK_CHECK_INTERVAL;
    }
  };

  DetachedFunction(1, func);
  m_fallbackTimerLaunched = true;
}

void Node::FallbackTimerPulse() {
  if (!ENABLE_FALLBACK) {
    return;
  }
  lock_guard<mutex> g(m_mutexFallbackTimer);
  m_fallbackTimer = 0;
  m_fallbackStarted = false;
}

void Node::FallbackStop() {
  if (!ENABLE_FALLBACK) {
    return;
  }
  lock_guard<mutex> g(m_mutexFallbackTimer);
  m_runFallback = false;
}

bool Node::ComposeFallbackBlock() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComputeNewFallbackLeader not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  LOG_GENERAL(INFO, "Composing new fallback block with consensus Leader ID at "
                        << m_consensusLeaderID);

  Peer leaderNetworkInfo;
  if (m_myShardMembers->at(m_consensusLeaderID).second == Peer()) {
    leaderNetworkInfo = m_mediator.m_selfPeer;
  } else {
    leaderNetworkInfo = m_myShardMembers->at(m_consensusLeaderID).second;
  }
  LOG_GENERAL(INFO, "m_myShardMembers->at(m_consensusLeaderID).second: "
                        << m_myShardMembers->at(m_consensusLeaderID).second);

  LOG_GENERAL(INFO, "m_mediator.m_selfPeer: " << m_mediator.m_selfPeer);
  LOG_GENERAL(INFO, "LeaderNetworkInfo: " << leaderNetworkInfo);

  CommitteeHash committeeHash;
  if (!Messenger::GetShardHash(m_mediator.m_ds->m_shards.at(m_myshardId),
                               committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetShardHash failed.");
    return false;
  }

  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      m_mediator.m_blocklinkchain.GetLatestBlockLink());

  lock_guard<mutex> g(m_mutexPendingFallbackBlock);

  // To-do: Handle exceptions.
  m_pendingFallbackBlock.reset(new FallbackBlock(
      FallbackBlockHeader(
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
              1,
          m_mediator.m_currentEpochNum, m_fallbackState,
          {AccountStore::GetInstance().GetStateRootHash()}, m_consensusLeaderID,
          leaderNetworkInfo, m_myShardMembers->at(m_consensusLeaderID).first,
          m_myshardId, FALLBACKBLOCK_VERSION, committeeHash, prevHash),
      CoSignatures()));

  return true;
}

void Node::RunConsensusOnFallback() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnFallback not expected "
                "to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  SetLastKnownGoodState();
  SetState(FALLBACK_CONSENSUS_PREP);

  // Upon consensus object creation failure, one should not return from the
  // function, but rather wait for fallback.
  bool ConsensusObjCreation = true;

  if (m_isPrimary) {
    ConsensusObjCreation = RunConsensusOnFallbackWhenLeader();
    if (!ConsensusObjCreation) {
      LOG_GENERAL(WARNING, "Error after RunConsensusOnFallbackWhenShardLeader");
    }
  } else {
    ConsensusObjCreation = RunConsensusOnFallbackWhenBackup();
    if (!ConsensusObjCreation) {
      LOG_GENERAL(WARNING, "Error after RunConsensusOnFallbackWhenShardBackup");
    }
  }

  if (ConsensusObjCreation) {
    SetState(FALLBACK_CONSENSUS);
    cv_fallbackConsensusObj.notify_all();
  }
}

bool Node::RunConsensusOnFallbackWhenLeader() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::"
                "RunConsensusOnFallbackWhenLeader not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am the fallback leader node. Announcing to the rest.");

  {
    lock_guard<mutex> g(m_mutexShardMember);

    if (!ComposeFallbackBlock()) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Node::RunConsensusOnFallbackWhenLeader failed.");
      return false;
    }

    // Create new consensus object
    m_consensusBlockHash =
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

    m_consensusObject.reset(new ConsensusLeader(
        m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
        m_consensusBlockHash, m_consensusMyID, m_mediator.m_selfKey.first,
        *m_myShardMembers, static_cast<uint8_t>(NODE),
        static_cast<uint8_t>(FALLBACKCONSENSUS), NodeCommitFailureHandlerFunc(),
        ShardCommitFailureHandlerFunc()));
  }

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Error: Unable to create consensus leader object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  bytes m;
  {
    lock_guard<mutex> g(m_mutexPendingFallbackBlock);
    m_pendingFallbackBlock->Serialize(m, 0);
  }

  std::this_thread::sleep_for(std::chrono::seconds(FALLBACK_EXTRA_TIME));

  auto announcementGeneratorFunc =
      [this](bytes& dst, unsigned int offset, const uint32_t consensusID,
             const uint64_t blockNumber, const bytes& blockHash,
             const uint16_t leaderID, const PairOfKey& leaderKey,
             bytes& messageToCosign) mutable -> bool {
    lock_guard<mutex> g(m_mutexPendingFallbackBlock);
    return Messenger::SetNodeFallbackBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_pendingFallbackBlock, messageToCosign);
  };

  cl->StartConsensus(announcementGeneratorFunc);

  return true;
}

bool Node::RunConsensusOnFallbackWhenBackup() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::RunConsensusOnFallbackWhenBackup not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am a fallback backup node. Waiting for Fallback announcement.");

  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  auto func = [this](const bytes& input, unsigned int offset, bytes& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const bytes& blockHash, const uint16_t leaderID,
                     const PubKey& leaderKey,
                     bytes& messageToCosign) mutable -> bool {
    return FallbackValidator(input, offset, errorMsg, consensusID, blockNumber,
                             blockHash, leaderID, leaderKey, messageToCosign);
  };

  {
    lock_guard<mutex> g(m_mutexShardMember);
    m_consensusObject.reset(new ConsensusBackup(
        m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
        m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
        m_mediator.m_selfKey.first, *m_myShardMembers,
        static_cast<uint8_t>(NODE), static_cast<uint8_t>(FALLBACKCONSENSUS),
        func));
  }

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Error: Unable to create consensus backup object");
    return false;
  }

  return true;
}
