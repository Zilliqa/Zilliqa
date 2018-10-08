/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

using namespace std;

void Node::ProcessFallbackConsensusWhenDone() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessViewChangeConsensusWhenDone not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Fallback consensus is DONE!!!");

  lock_guard<mutex> g(m_mutexPendingFallbackBlock);

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
  }

  vector<unsigned char> message;
  m_pendingFallbackBlock->GetHeader().Serialize(message, 0);
  m_pendingFallbackBlock->GetCS1().Serialize(message,
                                             FallbackBlockHeader::SIZE);
  BitVector::SetBitVector(message, FallbackBlockHeader::SIZE + BLOCK_SIG_SIZE,
                          m_pendingFallbackBlock->GetB1());
  if (not Schnorr::GetInstance().Verify(message, 0, message.size(),
                                        m_pendingFallbackBlock->GetCS2(),
                                        *aggregatetdKey)) {
    LOG_GENERAL(WARNING, "cosig verification fail");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return;
  }

  // StoreFallbackBlockToStorage(); TODO

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
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "After fallback, I am the ds leader!");
      m_mediator.m_ds->m_mode = DirectoryService::PRIMARY_DS;

      for (const auto& node : *m_myShardMembers) {
        if (node.second != Peer()) {
          m_mediator.m_DSCommittee->push_back(node);
        } else {
          m_mediator.m_DSCommittee->push_front(node);
        }
      }
      m_mediator.m_ds->m_consensusMyID = 0;
    } else {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "After fallback, I am a ds backup");
      m_mediator.m_ds->m_mode = DirectoryService::BACKUP_DS;

      unsigned int count = 1;
      for (const auto& node : *m_myShardMembers) {
        if (node.second != leaderNetworkInfo) {
          m_mediator.m_DSCommittee->push_back(node);
          if (node.second == Peer()) {
            m_mediator.m_ds->m_consensusMyID = count;
          }
          count++;
        } else {
          m_mediator.m_DSCommittee->push_front(node);
        }
      }
    }

    LOG_GENERAL(
        INFO, "My New DS consensusID is " << m_mediator.m_ds->m_consensusMyID);
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

    StoreState();

    if (!LOOKUP_NODE_MODE) {
      BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                  {'0'});
    }

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

  // Broadcasting fallback block to lookup nodes
  vector<unsigned char> fallbackblock_message = {
      MessageType::NODE, NodeInstructionType::FALLBACKBLOCK};

  if (!Messenger::SetNodeFallbackBlock(fallbackblock_message,
                                       MessageOffset::BODY,
                                       *m_pendingFallbackBlock)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetNodeFallbackBlock failed.");
    return;
  }

  unsigned int nodeToSendToLookUpLo = COMM_SIZE / 4;
  unsigned int nodeToSendToLookUpHi =
      nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

  if (m_mediator.m_ds->m_consensusMyID > nodeToSendToLookUpLo &&
      m_mediator.m_ds->m_consensusMyID < nodeToSendToLookUpHi) {
    m_mediator.m_lookup->SendMessageToLookupNodes(fallbackblock_message);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the part of the subset of newly DS committee after fallback "
              "that have sent the FallbackBlock to the lookup nodes");
  }

  // Broadcasting fallback block to nodes in other shard
  unsigned int my_DS_cluster_num;
  unsigned int my_shards_lo;
  unsigned int my_shards_hi;

  m_mediator.m_ds->DetermineShardsToSendBlockTo(my_DS_cluster_num, my_shards_lo,
                                                my_shards_hi);
  m_mediator.m_ds->SendBlockToShardNodes(my_DS_cluster_num, my_shards_lo,
                                         my_shards_hi, fallbackblock_message);
}

bool Node::ProcessFallbackConsensus(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from) {
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
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Successfully transit to fallback consensus or I am in the "
                  "correct state.");
      } else {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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
            if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC) {
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

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Consensus state = " << m_consensusObject->GetStateString());

  if (state == ConsensusCommon::State::DONE) {
    ProcessFallbackConsensusWhenDone();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Fallback consensus is DONE!!!");
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "No consensus reached. Will attempt to do fallback again");
    return false;
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << state);
    cv_processConsensusMessage.notify_all();
  }
  return true;
}