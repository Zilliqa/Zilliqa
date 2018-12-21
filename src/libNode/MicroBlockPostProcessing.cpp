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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
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

bool Node::ComposeMicroBlockMessageForSender(
    vector<unsigned char>& microblock_message) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComposeMicroBlockMessageForSender not expected to be "
                "called from LookUp node.");
    return false;
  }

  microblock_message.clear();

  microblock_message = {MessageType::DIRECTORY,
                        DSInstructionType::MICROBLOCKSUBMISSION};
  vector<unsigned char> stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetDSMicroBlockSubmission(
          microblock_message, MessageOffset::BODY,
          DirectoryService::SUBMITMICROBLOCKTYPE::SHARDMICROBLOCK,
          m_mediator.m_currentEpochNum, {*m_microblock}, {stateDelta})) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetDSMicroBlockSubmission failed.");
    return false;
  }

  return true;
}

bool Node::ProcessMicroblockConsensus(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessMicroblockConsensus not expected to be "
                "called from LookUp node.");
    return true;
  }

  uint32_t consensus_id = 0;

  if (!m_consensusObject->GetConsensusID(message, offset, consensus_id)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "GetConsensusID failed.");
    return false;
  }

  if (m_state != MICROBLOCK_CONSENSUS) {
    lock_guard<mutex> h(m_mutexMicroBlockConsensusBuffer);

    m_microBlockConsensusBuffer[consensus_id].push_back(
        make_pair(from, message));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Process micro block arrived early, saved to buffer");
  } else {
    if (consensus_id < m_mediator.m_consensusID) {
      LOG_GENERAL(WARNING, "Consensus ID in message ("
                               << consensus_id << ") is smaller than current ("
                               << m_mediator.m_consensusID << ")");
      return false;
    } else if (consensus_id > m_mediator.m_consensusID) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Buffer microblock with larger consensus ID ("
                    << consensus_id << "), current ("
                    << m_mediator.m_consensusID << ")");

      lock_guard<mutex> h(m_mutexMicroBlockConsensusBuffer);

      m_microBlockConsensusBuffer[consensus_id].push_back(
          make_pair(from, message));
    } else {
      return ProcessMicroblockConsensusCore(message, offset, from);
    }
  }

  return true;
}

void Node::CommitMicroBlockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);

  for (const auto i : m_microBlockConsensusBuffer[m_mediator.m_consensusID]) {
    auto runconsensus = [this, i]() {
      ProcessMicroblockConsensusCore(i.second, MessageOffset::BODY, i.first);
    };
    DetachedFunction(1, runconsensus);
  }
}

void Node::CleanMicroblockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);
  m_microBlockConsensusBuffer.clear();
}

bool Node::ProcessMicroblockConsensusCore(const vector<unsigned char>& message,
                                          unsigned int offset,
                                          const Peer& from) {
  LOG_MARKER();

  if (!CheckState(PROCESS_MICROBLOCKCONSENSUS)) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
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

    // TODO: provide interface in DataSender instead of repopulating the DS into
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
        [this](vector<unsigned char>& microblock_message) -> bool {
      return ComposeMicroBlockMessageForSender(microblock_message);
    };
    {
      lock_guard<mutex> g(m_mutexShardMember);
      DataSender::GetInstance().SendDataToOthers(
          *m_microblock, *m_myShardMembers, ds_shards,
          m_mediator.m_lookup->GetLookupNodes(),
          m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(),
          composeMicroBlockMessageForSender, nullptr);
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "fetching missing txn timeout");
      } else {
        // Re-run consensus
        m_consensusObject->RecoveryAndProcessFromANewState(
            ConsensusCommon::INITIAL);

        auto reprocessconsensus = [this, message, offset, from]() {
          ProcessMicroblockConsensusCore(message, offset, from);
        };
        DetachedFunction(1, reprocessconsensus);
        return true;
      }
    } else {
    }

    // return false;
    // TODO: Optimize state transition.
    LOG_GENERAL(WARNING, "ConsensusCommon::State::ERROR here, but we move on.");

    SetState(WAITING_FINALBLOCK);  // Move on to next Epoch.
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "If I received a new Finalblock from DS committee. I will "
              "still process it");

    lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
    cv_FBWaitMB.notify_all();
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << m_consensusObject->GetStateString());

    cv_processConsensusMessage.notify_all();
  }
  return true;
}
