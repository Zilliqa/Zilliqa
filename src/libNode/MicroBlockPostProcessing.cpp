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

#include <boost/multiprecision/cpp_int.hpp>
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
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;
using namespace boost::multi_index;

void Node::SubmitMicroblockToDSCommittee() const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::SubmitMicroblockToDSCommittee not expected to be "
                "called from LookUp node.");
    return;
  }

  if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE) {
    return;
  }

  vector<unsigned char> microblock = {MessageType::DIRECTORY,
                                      DSInstructionType::MICROBLOCKSUBMISSION};
  const uint64_t& txBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  vector<unsigned char> stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetDSMicroBlockSubmission(
          microblock, MessageOffset::BODY,
          DirectoryService::SUBMITMICROBLOCKTYPE::SHARDMICROBLOCK, txBlockNum,
          {*m_microblock}, stateDelta)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetDSMicroBlockSubmission failed.");
    return;
  }

  LOG_STATE("[MICRO][" << std::setw(15) << std::left
                       << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                       << m_mediator.m_currentEpochNum << "] SENT");

  if (BROADCAST_GOSSIP_MODE) {
    P2PComm::GetInstance().SendRumorToForeignPeers(m_DSMBReceivers, microblock);
  } else {
    deque<Peer> peerList;

    for (auto const& i : *m_mediator.m_DSCommittee) {
      peerList.push_back(i.second);
    }
    P2PComm::GetInstance().SendBroadcastMessage(peerList, microblock);
  }
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
              "Process micro block arrived earlier, saved to buffer");
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

  for (const auto& i : m_microBlockConsensusBuffer[m_mediator.m_consensusID]) {
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
            if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC) {
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
      LOG_STATE("[MICON][" << std::setw(15) << std::left
                           << m_mediator.m_selfPeer.GetPrintableIPAddress()
                           << "][" << m_mediator.m_currentEpochNum << "] DONE");

      // Multicast micro block to all DS nodes
      SubmitMicroblockToDSCommittee();
    }

    if (m_isMBSender) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Designated as Microblock sender");

      // Multicast micro block to all DS nodes
      SubmitMicroblockToDSCommittee();
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Micro block consensus "
                  << "is DONE!!! (Epoch " << m_mediator.m_currentEpochNum
                  << ")");
    m_lastMicroBlockCoSig.first = m_mediator.m_currentEpochNum;
    m_lastMicroBlockCoSig.second.SetCoSignatures(*m_consensusObject);

    SetState(WAITING_FINALBLOCK);

    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
      lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
      cv_FBWaitMB.notify_all();
    } else {
      lock_guard<mutex> g(
          m_mediator.m_ds->m_mutexPrepareRunFinalblockConsensus);
      if (!m_mediator.m_ds->m_startedRunFinalblockConsensus) {
        m_mediator.m_ds->m_stateDeltaFromShards.clear();
        AccountStore::GetInstance().SerializeDelta();
        AccountStore::GetInstance().GetSerializedDelta(
            m_mediator.m_ds->m_stateDeltaFromShards);
        m_mediator.m_ds->SaveCoinbase(m_microblock->GetB1(),
                                      m_microblock->GetB2(),
                                      m_microblock->GetHeader().GetShardId());
        m_mediator.m_ds->cv_scheduleFinalBlockConsensus.notify_all();
        {
          lock_guard<mutex> g(m_mediator.m_ds->m_mutexMicroBlocks);
          m_mediator.m_ds->m_microBlocks[m_mediator.m_currentEpochNum].emplace(
              *m_microblock);
        }
        m_mediator.m_ds->m_toSendTxnToLookup = true;
      }
      m_mediator.m_ds->RunConsensusOnFinalBlock();
    }
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Oops, no consensus reached - what to do now???");

    if (m_consensusObject->GetConsensusErrorCode() ==
        ConsensusCommon::MISSING_TXN) {
      // Missing txns in microblock proposed by leader. Will attempt to fetch
      // missing txns from leader, set to a valid state to accept cosig1 and
      // cosig2
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << (m_consensusObject->GetConsensusErrorMsg()));

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

        auto rerunconsensus = [this, message, offset, from]() {
          ProcessMicroblockConsensus(message, offset, from);
        };
        DetachedFunction(1, rerunconsensus);
        return true;
      }
    } else {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - unhandled consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << m_consensusObject->GetConsensusErrorMsg());
    }

    // return false;
    // TODO: Optimize state transition.
    LOG_GENERAL(WARNING, "ConsensusCommon::State::ERROR here, but we move on.");

    SetState(WAITING_FINALBLOCK);  // Move on to next Epoch.
    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "If I received a new Finalblock from DS committee. I will "
                "still process it");

      lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
      cv_FBWaitMB.notify_all();
    } else {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "DS Microblock failed, discard changes on microblock and "
                "proceed to finalblock consensus");
      m_mediator.m_ds->cv_scheduleFinalBlockConsensus.notify_all();
      m_mediator.m_ds->RunConsensusOnFinalBlock(true);
    }
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << m_consensusObject->GetStateString());

    cv_processConsensusMessage.notify_all();
  }
  return true;
}