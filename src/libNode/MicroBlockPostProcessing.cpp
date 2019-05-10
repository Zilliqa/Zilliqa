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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libPOW/pow.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/RootComputation.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;
using namespace boost::multi_index;

bool Node::ComposeMicroBlockMessageForSender(bytes& microblock_message) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComposeMicroBlockMessageForSender not expected to be "
                "called from LookUp node.");
    return false;
  }

  microblock_message.clear();

  microblock_message = {MessageType::DIRECTORY,
                        DSInstructionType::MICROBLOCKSUBMISSION};
  bytes stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetDSMicroBlockSubmission(
          microblock_message, MessageOffset::BODY,
          DirectoryService::SUBMITMICROBLOCKTYPE::SHARDMICROBLOCK,
          m_mediator.m_currentEpochNum, {*m_microblock}, {stateDelta},
          m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetDSMicroBlockSubmission failed.");
    return false;
  }

  return true;
}

bool Node::ProcessMicroBlockConsensus(const bytes& message, unsigned int offset,
                                      const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessMicroBlockConsensus not expected to be "
                "called from LookUp node.");
    return true;
  }

  uint32_t consensus_id = 0;
  bytes reserialized_message;
  PubKey senderPubKey;

  if (!m_consensusObject->PreProcessMessage(
          message, offset, consensus_id, senderPubKey, reserialized_message)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "PreProcessMessage failed");
    return false;
  }

  if (!IsShardNode(senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "ProcessMicroBlockConsensus signed by non shard member");
  }

  if (m_state != MICROBLOCK_CONSENSUS) {
    AddToMicroBlockConsensusBuffer(consensus_id, reserialized_message, offset,
                                   from, senderPubKey);

    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Process micro block arrived early, saved to buffer");
  } else {
    if (consensus_id < m_mediator.m_consensusID) {
      LOG_GENERAL(WARNING, "Consensus ID in message ("
                               << consensus_id << ") is smaller than current ("
                               << m_mediator.m_consensusID << ")");
      return false;
    } else if (consensus_id > m_mediator.m_consensusID) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Buffer microblock with larger consensus ID ("
                    << consensus_id << "), current ("
                    << m_mediator.m_consensusID << ")");

      AddToMicroBlockConsensusBuffer(consensus_id, reserialized_message, offset,
                                     from, senderPubKey);
    } else {
      return ProcessMicroBlockConsensusCore(reserialized_message, offset, from);
    }
  }

  return true;
}

void Node::CommitMicroBlockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);

  for (const auto i : m_microBlockConsensusBuffer[m_mediator.m_consensusID]) {
    auto runconsensus = [this, i]() {
      ProcessMicroBlockConsensusCore(std::get<NODE_MSG>(i), MessageOffset::BODY,
                                     std::get<NODE_PEER>(i));
    };
    DetachedFunction(1, runconsensus);
  }
}

void Node::AddToMicroBlockConsensusBuffer(uint32_t consensusId,
                                          const bytes& message,
                                          unsigned int offset, const Peer& peer,
                                          const PubKey& senderPubKey) {
  if (message.size() <= offset) {
    LOG_GENERAL(WARNING, "The message size " << message.size()
                                             << " is less than the offset "
                                             << offset);
    return;
  }
  lock_guard<mutex> h(m_mutexMicroBlockConsensusBuffer);
  auto& vecNodeMsg = m_microBlockConsensusBuffer[consensusId];
  const auto consensusMsgType = message[offset];
  // Check if the node send the same consensus message already, prevent
  // malicious node send unlimited message to crash the other nodes
  if (vecNodeMsg.end() !=
      std::find_if(
          vecNodeMsg.begin(), vecNodeMsg.end(),
          [senderPubKey, consensusMsgType, offset](const NodeMsg& nodeMsg) {
            return senderPubKey == std::get<NODE_PUBKEY>(nodeMsg) &&
                   consensusMsgType == std::get<NODE_MSG>(nodeMsg)[offset];
          })) {
    LOG_GENERAL(
        WARNING,
        "The node "
            << senderPubKey
            << " already send micro block consensus message for consensus id "
            << consensusId << " message type "
            << std::to_string(consensusMsgType));
    return;
  }

  vecNodeMsg.push_back(make_tuple(senderPubKey, peer, message));
}

void Node::CleanMicroblockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);
  m_microBlockConsensusBuffer.clear();
}

bool Node::ProcessMicroBlockConsensusCore(const bytes& message,
                                          unsigned int offset,
                                          const Peer& from) {
  LOG_MARKER();

  if (!CheckState(PROCESS_MICROBLOCKCONSENSUS)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Not in MICROBLOCK_CONSENSUS state");
    return false;
  }

  // Consensus message must be processed in order. The following will block till
  // it is the right order.
  std::unique_lock<mutex> cv_lk(m_mutexProcessConsensusMessage);
  if (cv_processConsensusMessage.wait_for(
          cv_lk, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
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
                          "m_consensusObject should have been created "
                          "but it is not")
              return false;
            }
            return m_consensusObject->CanProcessMessage(message, offset);
          })) {
    // Correct order preserved
  } else {
    LOG_GENERAL(WARNING,
                "Timeout while waiting for correct order of consensus "
                "messages");
    return false;
  }

  lock_guard<mutex> g(m_mutexConsensus);

  if (!CheckState(PROCESS_MICROBLOCKCONSENSUS)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Not in MICROBLOCK_CONSENSUS state");
    return false;
  }

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();

  if (state == ConsensusCommon::State::DONE) {
    // Update the micro block with the co-signatures from the consensus
    m_microblock->SetCoSignatures(*m_consensusObject);

    if (m_isPrimary) {
      LOG_STATE("[MICON][" << setw(15) << left
                           << m_mediator.m_selfPeer.GetPrintableIPAddress()
                           << "][" << m_mediator.m_currentEpochNum << "]["
                           << m_myshardId << "] DONE");
    }

    // shard
    DequeOfShard ds_shards;
    Shard ds_shard;
    {
      lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
      for (const auto& entry : *m_mediator.m_DSCommittee) {
        ds_shard.emplace_back(entry.first, entry.second, 0);
      }
    }
    ds_shards.emplace_back(ds_shard);

    auto composeMicroBlockMessageForSender =
        [this](bytes& microblock_message) -> bool {
      return ComposeMicroBlockMessageForSender(microblock_message);
    };

    unordered_map<uint32_t, BlockBase> t_blocks;
    if (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum() ==
        m_mediator.m_currentEpochNum) {
      t_blocks.emplace(0, m_mediator.m_dsBlockChain.GetLastBlock());
    } else {
      t_blocks.emplace(0, m_mediator.m_txBlockChain.GetLastBlock());
    }

    {
      lock_guard<mutex> g(m_mutexShardMember);
      DataSender::GetInstance().SendDataToOthers(
          *m_microblock, *m_myShardMembers, ds_shards, t_blocks,
          m_mediator.m_lookup->GetLookupNodes(),
          m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(),
          m_consensusMyID, composeMicroBlockMessageForSender, false, nullptr);
    }

    LOG_STATE(
        "[MIBLK]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] AFTER SENDING MIBLK");

    m_lastMicroBlockCoSig.first = m_mediator.m_currentEpochNum;
    m_lastMicroBlockCoSig.second = move(
        CoSignatures(m_consensusObject->GetCS1(), m_consensusObject->GetB1(),
                     m_consensusObject->GetCS2(), m_consensusObject->GetB2()));

    SetState(WAITING_FINALBLOCK);

    lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
    cv_FBWaitMB.notify_all();
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Oops, no consensus reached - consensus error. "
              "error number: "
                  << to_string(m_consensusObject->GetConsensusErrorCode())
                  << " error message: "
                  << (m_consensusObject->GetConsensusErrorMsg()));

    if (m_consensusObject->GetConsensusErrorCode() ==
        ConsensusCommon::MISSING_TXN) {
      // Missing txns in microblock proposed by leader. Will attempt to fetch
      // missing txns from leader, set to a valid state to accept cosig1 and
      // cosig2
      LOG_GENERAL(INFO, "Start pending for fetching missing txns")

      // Block till txn is fetched
      unique_lock<mutex> lock(m_mutexCVMicroBlockMissingTxn);
      if (cv_MicroBlockMissingTxn.wait_for(
              lock, chrono::seconds(FETCHING_MISSING_DATA_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "fetching missing txn timeout");
      } else {
        // Re-run consensus
        m_consensusObject->RecoveryAndProcessFromANewState(
            ConsensusCommon::INITIAL);

        auto reprocessconsensus = [this, message, offset, from]() {
          ProcessTransactionWhenShardBackup();
          ProcessMicroBlockConsensusCore(message, offset, from);
        };
        DetachedFunction(1, reprocessconsensus);
        return true;
      }
    }

    // TODO: Optimize state transition.
    LOG_GENERAL(WARNING, "ConsensusCommon::State::ERROR here, but we move on.");

    SetState(WAITING_FINALBLOCK);  // Move on to next Epoch.
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "If I received a new Finalblock from DS committee. I will "
              "still process it");

    lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
    cv_FBWaitMB.notify_all();
  } else {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Consensus = " << m_consensusObject->GetStateString());

    cv_processConsensusMessage.notify_all();
  }
  return true;
}
