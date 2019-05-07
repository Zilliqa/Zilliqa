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

#include <algorithm>
#include <chrono>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

using namespace std;

bool Node::ComposeFallbackBlockMessageForSender(
    bytes& fallbackblock_message) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComposeFallbackBlockMessageForSender not "
                "expected to be called from LookUp node.");
    return false;
  }
  fallbackblock_message.clear();

  fallbackblock_message = {MessageType::NODE,
                           NodeInstructionType::FALLBACKBLOCK};

  if (!Messenger::SetNodeFallbackBlock(fallbackblock_message,
                                       MessageOffset::BODY,
                                       *m_pendingFallbackBlock)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetNodeFallbackBlock failed.");
    return false;
  }

  return true;
}

void Node::ProcessFallbackConsensusWhenDone() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessFallbackConsensusWhenDone not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Fallback consensus DONE");

  lock(m_mutexPendingFallbackBlock, m_mutexShardMember);
  lock_guard<mutex> g(m_mutexPendingFallbackBlock, adopt_lock);
  lock_guard<mutex> g2(m_mutexShardMember, adopt_lock);

  m_pendingFallbackBlock->SetCoSignatures(*m_consensusObject);

  unsigned int index = 0;
  unsigned int count = 0;

  vector<PubKey> keys;
  for (auto const& kv : *m_myShardMembers) {
    if (m_pendingFallbackBlock->GetB2().at(index)) {
      keys.emplace_back(kv.first);
      count++;
    }
    index++;
  }

  // Verify cosig agains fallbackblock
  shared_ptr<PubKey> aggregatetdKey = MultiSig::AggregatePubKeys(keys);
  if (aggregatetdKey == nullptr) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    return;
  }

  bytes message;
  if (!m_pendingFallbackBlock->GetHeader().Serialize(message, 0)) {
    LOG_GENERAL(WARNING, "FallbackBlockHeader serialization failed");
    return;
  }
  m_pendingFallbackBlock->GetCS1().Serialize(message, message.size());
  BitVector::SetBitVector(message, message.size(),
                          m_pendingFallbackBlock->GetB1());
  if (!MultiSig::GetInstance().MultiSigVerify(message, 0, message.size(),
                                              m_pendingFallbackBlock->GetCS2(),
                                              *aggregatetdKey)) {
    LOG_GENERAL(WARNING, "cosig verification fail");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return;
  }

  uint64_t latestInd = m_mediator.m_blocklinkchain.GetLatestIndex() + 1;
  m_mediator.m_blocklinkchain.AddBlockLink(
      latestInd, m_pendingFallbackBlock->GetHeader().GetFallbackDSEpochNo(),
      BlockType::FB, m_pendingFallbackBlock->GetBlockHash());

  bytes dst;

  FallbackBlockWShardingStructure fbblockwshards(*m_pendingFallbackBlock,
                                                 m_mediator.m_ds->m_shards);
  if (fbblockwshards.Serialize(dst, 0)) {
    if (!BlockStorage::GetBlockStorage().PutFallbackBlock(
            m_pendingFallbackBlock->GetBlockHash(), dst)) {
      LOG_GENERAL(WARNING, "Unable to store FallbackBlock "
                               << m_pendingFallbackBlock->GetBlockHash());
      return;
    }
  } else {
    LOG_GENERAL(WARNING, "Failed to Serialize");
  }

  Peer leaderNetworkInfo =
      m_pendingFallbackBlock->GetHeader().GetLeaderNetworkInfo();
  Peer expectedLeader;
  if (m_myShardMembers->at(m_consensusLeaderID).second == Peer()) {
    // I am 0.0.0.0
    expectedLeader = m_mediator.m_selfPeer;
  } else {
    expectedLeader = m_myShardMembers->at(m_consensusLeaderID).second;
  }

  if (expectedLeader == leaderNetworkInfo) {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

    m_mediator.m_DSCommittee->clear();

    if (leaderNetworkInfo == m_mediator.m_selfPeer) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "After fallback, I am the ds leader!");
      m_mediator.m_ds->m_mode = DirectoryService::PRIMARY_DS;

      for (const auto& node : *m_myShardMembers) {
        if (node.second != Peer()) {
          m_mediator.m_DSCommittee->push_back(node);
        } else {
          m_mediator.m_DSCommittee->push_front(node);
        }
      }
      m_mediator.m_ds->SetConsensusMyID(0);
    } else {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "After fallback, I am a ds backup");
      m_mediator.m_ds->m_mode = DirectoryService::BACKUP_DS;

      unsigned int count = 1;
      for (const auto& node : *m_myShardMembers) {
        if (node.second != leaderNetworkInfo) {
          m_mediator.m_DSCommittee->push_back(node);
          if (node.second == Peer()) {
            m_mediator.m_ds->SetConsensusMyID(count);
          }
          count++;
        } else {
          m_mediator.m_DSCommittee->push_front(node);
        }
      }
    }

    LOG_GENERAL(INFO, "My New DS consensusID is "
                          << m_mediator.m_ds->GetConsensusMyID());
    LOG_GENERAL(INFO, "New ds committee after fallback: ");
    for (const auto& node : *m_mediator.m_DSCommittee) {
      LOG_GENERAL(INFO, node.second);
    }

    // Clean processedTxn may have been produced during last microblock
    // consensus
    {
      lock_guard<mutex> g(m_mutexProcessedTransactions);
      m_processedTransactions.erase(m_mediator.m_currentEpochNum);
    }

    AccountStore::GetInstance().InitTemp();

    auto writeStateToDisk = [this]() mutable -> void {
      if (!AccountStore::GetInstance().MoveUpdatesToDisk()) {
        LOG_GENERAL(WARNING, "MoveUpdatesToDisk failed, what to do?");
        return;
      }
      if (!BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                       {'0'})) {
        LOG_GENERAL(WARNING,
                    "BlockStorage::PutMetadata (DSINCOMPLETED) '0' failed");
        return;
      }
      LOG_STATE("[FLBLK][" << setw(15) << left
                           << m_mediator.m_selfPeer.GetPrintableIPAddress()
                           << "]["
                           << m_mediator.m_txBlockChain.GetLastBlock()
                                      .GetHeader()
                                      .GetBlockNum() +
                                  1
                           << "] FINISH WRITE STATE TO DISK");
    };
    DetachedFunction(1, writeStateToDisk);

    SetState(POW_SUBMISSION);

    // Detach a thread, Pending for POW Submission and RunDSBlockConsensus
    auto func = [this]() -> void {
      m_mediator.m_ds->StartNewDSEpochConsensus(true);
    };
    DetachedFunction(1, func);
  }

  // Update m_shards
  for (unsigned int i = 0; i <= m_myshardId; i++) {
    m_mediator.m_ds->m_shards.pop_front();
  }

  auto composeFallbackBlockMessageForSender =
      [this](bytes& fallback_message) -> bool {
    return ComposeFallbackBlockMessageForSender(fallback_message);
  };

  {
    DataSender::GetInstance().SendDataToOthers(
        *m_microblock, *m_myShardMembers, m_mediator.m_ds->m_shards, {},
        m_mediator.m_lookup->GetLookupNodes(),
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(),
        m_consensusMyID, composeFallbackBlockMessageForSender);
  }
}

bool Node::ProcessFallbackConsensus(const bytes& message, unsigned int offset,
                                    const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessFallbackConsensus not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexConsensus);

    if (!CheckState(PROCESS_FALLBACKCONSENSUS)) {
      std::unique_lock<std::mutex> cv_lk(m_MutexCVFallbackConsensusObj);
      if (cv_fallbackConsensusObj.wait_for(
              cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
              [this] { return CheckState(PROCESS_FALLBACKCONSENSUS); })) {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Successfully transit to fallback consensus or I am in the "
                  "correct state.");
      } else {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "Time out while waiting for state transition to fallback "
                  "consensus and "
                  "consensus object creation. Most likely fallback didn't "
                  "occur. A malicious node may be trying to initate "
                  "fallback.");
        return false;
      }
    }
  }

  // Consensus messages must be processed in correct sequence as they come in
  // It is possible for ANNOUNCE to arrive before correct DS state
  // In that case, state transition will occurs and ANNOUNCE will be processed.
  std::unique_lock<mutex> cv_lk_con_msg(m_mutexProcessConsensusMessage);
  if (cv_processConsensusMessage.wait_for(
          cv_lk_con_msg, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
          [this, message, offset]() -> bool {
            lock_guard<mutex> g(m_mutexConsensus);
            if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
              LOG_GENERAL(WARNING,
                          "The node started the process of rejoining, "
                          "Ignore rest of "
                          "consensus msg.")
              return false;
            }

            if (m_consensusObject == nullptr) {
              LOG_GENERAL(WARNING,
                          "m_consensusObject is a nullptr. It has not "
                          "been initialized.")
              return false;
            }
            return m_consensusObject->CanProcessMessage(message, offset);
          })) {
    // Correct order preserved
  } else {
    LOG_GENERAL(WARNING,
                "Timeout while waiting for correct order of Fallback "
                "Block consensus "
                "messages");
    return false;
  }

  lock_guard<mutex> g(m_mutexConsensus);

  if (!CheckState(PROCESS_FALLBACKCONSENSUS)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Not in PROCESS_FALLBACKCONSENSUS state");
    return false;
  }

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Consensus = " << m_consensusObject->GetStateString());

  if (state == ConsensusCommon::State::DONE) {
    ProcessFallbackConsensusWhenDone();
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Fallback consensus DONE");
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "No consensus reached. Will attempt to do fallback again");
    return false;
  } else {
    cv_processConsensusMessage.notify_all();
  }
  return true;
}
