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

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/UpgradeManager.h"

using namespace std;
using namespace boost::multiprecision;

void DirectoryService::StoreFinalBlockToDisk() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::StoreFinalBlockToDisk not expected to "
                "be called from LookUp node.");
    return;
  }

  // Add finalblock to txblockchain
  m_mediator.m_node->AddBlock(*m_finalBlock);
  m_mediator.IncreaseEpochNum();

  // At this point, the transactions in the last Epoch is no longer useful, thus
  // erase.
  // m_mediator.m_node->EraseCommittedTransactions(m_mediator.m_currentEpochNum
  //                                               - 2);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Storing Tx Block Number: "
                << m_finalBlock->GetHeader().GetBlockNum() << " with Type: "
                << to_string(m_finalBlock->GetHeader().GetType())
                << ", Version: " << m_finalBlock->GetHeader().GetVersion()
                << ", Timestamp: " << m_finalBlock->GetTimestamp()
                << ", NumTxs: " << m_finalBlock->GetHeader().GetNumTxs());

  bytes serializedTxBlock;
  m_finalBlock->Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(
      m_finalBlock->GetHeader().GetBlockNum(), serializedTxBlock);

  bytes stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);
  BlockStorage::GetBlockStorage().PutStateDelta(
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
      stateDelta);
}

bool DirectoryService::ComposeFinalBlockMessageForSender(
    bytes& finalblock_message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComposeFinalBlockMessageForSender not "
                "expected to be called from LookUp node.");
    return false;
  }

  finalblock_message.clear();

  finalblock_message = {MessageType::NODE, NodeInstructionType::FINALBLOCK};

  const uint64_t dsBlockNumber =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  bytes stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetNodeFinalBlock(finalblock_message, MessageOffset::BODY,
                                    dsBlockNumber, m_mediator.m_consensusID,
                                    *m_finalBlock, stateDelta)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetNodeFinalBlock failed.");
    return false;
  }

  return true;
}

void DirectoryService::ProcessFinalBlockConsensusWhenDone() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessFinalBlockConsensusWhenDone not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Final block consensus is DONE!!!");

  // Clear microblock(s)
  // m_microBlocks.clear();

  // m_mediator.HeartBeatPulse();

  if (m_mode == PRIMARY_DS) {
    LOG_STATE(
        "[FBCON]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] DONE");
  }

  // Update the final block with the co-signatures from the consensus
  m_finalBlock->SetCoSignatures(*m_consensusObject);

  // Update the DS microblock with the same co-signatures from the consensus
  // If we don't do this, DataSender won't be able to process it
  m_mediator.m_node->m_microblock->SetCoSignatures(*m_consensusObject);

  bool isVacuousEpoch = m_mediator.GetIsVacuousEpoch();

  // StoreMicroBlocksToDisk();
  StoreFinalBlockToDisk();

  if (isVacuousEpoch) {
    AccountStore::GetInstance().MoveUpdatesToDisk();
    BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED, {'0'});
  } else {
    // Coinbase
    SaveCoinbase(m_finalBlock->GetB1(), m_finalBlock->GetB2(),
                 CoinbaseReward::FINALBLOCK_REWARD,
                 m_mediator.m_currentEpochNum);
    m_totalTxnFees += m_finalBlock->GetHeader().GetRewards();
  }

  m_mediator.UpdateDSBlockRand();
  m_mediator.UpdateTxBlockRand();

  auto composeFinalBlockMessageForSender = [this](bytes& message) -> bool {
    return ComposeFinalBlockMessageForSender(message);
  };

  // Acquire shard receivers cosigs from MicroBlocks
  unordered_map<uint32_t, BlockBase> t_microBlocks;
  const auto& microBlocks = m_microBlocks
      [m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()];
  for (const auto& microBlock : microBlocks) {
    t_microBlocks.emplace(microBlock.GetHeader().GetShardId(), microBlock);
  }

  DataSender::GetInstance().SendDataToOthers(
      *m_finalBlock, *m_mediator.m_DSCommittee, m_shards, t_microBlocks,
      m_mediator.m_lookup->GetLookupNodes(),
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(), m_consensusMyID,
      composeFinalBlockMessageForSender);

  LOG_STATE(
      "[FLBLK]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] AFTER SENDING FLBLK");

  if (m_mediator.m_node->m_microblock != nullptr && !isVacuousEpoch) {
    m_mediator.m_node->UpdateProcessedTransactions();
    if (m_mediator.m_node->m_microblock->GetHeader().GetTxRootHash() !=
        TxnHash()) {
      m_mediator.m_node->CallActOnFinalblock();
    }
  }

  {
    lock_guard<mutex> g(m_mediator.m_mutexCurSWInfo);
    if (isVacuousEpoch && m_mediator.m_curSWInfo.GetUpgradeDS() - 1 ==
                              m_mediator.m_dsBlockChain.GetLastBlock()
                                  .GetHeader()
                                  .GetBlockNum()) {
      auto func = [this]() mutable -> void {
        UpgradeManager::GetInstance().ReplaceNode(m_mediator);
      };
      DetachedFunction(1, func);
    }
  }

  AccountStore::GetInstance().InitTemp();
  AccountStore::GetInstance().InitReversibles();
  m_stateDeltaFromShards.clear();
  m_allPoWConns.clear();
  ClearDSPoWSolns();
  ResetPoWSubmissionCounter();

  auto func = [this, isVacuousEpoch]() mutable -> void {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "START OF a new EPOCH");
    if (isVacuousEpoch) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "[PoW needed]");

      StartNewDSEpochConsensus();
    } else {
      m_mediator.m_node->UpdateStateForNextConsensusRound();
      SetState(MICROBLOCK_SUBMISSION);
      m_stopRecvNewMBSubmission = false;
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "[No PoW needed] Waiting for Microblock.");

      LOG_STATE("[MIBLKSWAIT][" << setw(15) << left
                                << m_mediator.m_selfPeer.GetPrintableIPAddress()
                                << "]["
                                << m_mediator.m_txBlockChain.GetLastBlock()
                                           .GetHeader()
                                           .GetBlockNum() +
                                       1
                                << "] BEGIN");

      auto func1 = [this]() mutable -> void {
        m_mediator.m_node->CommitTxnPacketBuffer();
      };
      DetachedFunction(1, func1);

      CommitMBSubmissionMsgBuffer();

      std::unique_lock<std::mutex> cv_lk(m_MutexScheduleDSMicroBlockConsensus);
      if (cv_scheduleDSMicroBlockConsensus.wait_for(
              cv_lk, std::chrono::seconds(MICROBLOCK_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_GENERAL(WARNING,
                    "Timeout: Didn't receive all Microblock. Proceeds "
                    "without it");

        LOG_STATE("[MIBLKSWAIT]["
                  << setw(15) << left
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                  << m_mediator.m_txBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetBlockNum() +
                         1
                  << "] TIMEOUT: Didn't receive all Microblock.");

        m_stopRecvNewMBSubmission = true;

        RunConsensusOnFinalBlock();
      }
    }
  };

  DetachedFunction(1, func);
}

bool DirectoryService::ProcessFinalBlockConsensus(const bytes& message,
                                                  unsigned int offset,
                                                  const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessFinalBlockConsensus not expected "
                "to be called from LookUp node.");
    return true;
  }

  uint32_t consensus_id = 0;

  if (!m_consensusObject->GetConsensusID(message, offset, consensus_id)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "GetConsensusID failed.");
    return false;
  }

  if (!CheckState(PROCESS_FINALBLOCKCONSENSUS)) {
    // don't buffer the Final block consensus message if i am non-ds node
    if (m_mode == Mode::IDLE) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Ignoring final block consensus message");
      return false;
    }
    // Only buffer the Final block consensus message if in the immediate states
    // before consensus, or when doing view change
    if (!((m_state == MICROBLOCK_SUBMISSION) ||
          (m_state == FINALBLOCK_CONSENSUS_PREP) ||
          (m_state == VIEWCHANGE_CONSENSUS))) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Ignoring final block consensus message");
      return false;
    }
    {
      lock_guard<mutex> h(m_mutexFinalBlockConsensusBuffer);
      m_finalBlockConsensusBuffer[consensus_id].push_back(
          make_pair(from, message));
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Process final block arrived early, saved to buffer");

    if (consensus_id == m_mediator.m_consensusID) {
      lock_guard<mutex> g(m_mutexPrepareRunFinalblockConsensus);
      cv_scheduleDSMicroBlockConsensus.notify_all();
      if (!m_stopRecvNewMBSubmission) {
        m_stopRecvNewMBSubmission = true;
      }
      cv_scheduleFinalBlockConsensus.notify_all();
      RunConsensusOnFinalBlock();
    }
  } else {
    if (consensus_id < m_mediator.m_consensusID) {
      LOG_GENERAL(WARNING, "Consensus ID in message ("
                               << consensus_id << ") is smaller than current ("
                               << m_mediator.m_consensusID << ")");
      return false;
    } else if (consensus_id > m_mediator.m_consensusID) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Buffer final block with larger consensus ID ("
                    << consensus_id << "), current ("
                    << m_mediator.m_consensusID << ")");

      {
        lock_guard<mutex> h(m_mutexFinalBlockConsensusBuffer);
        m_finalBlockConsensusBuffer[consensus_id].push_back(
            make_pair(from, message));
      }
    } else {
      return ProcessFinalBlockConsensusCore(message, offset, from);
    }
  }

  return true;
}

void DirectoryService::CommitFinalBlockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexFinalBlockConsensusBuffer);

  for (const auto& i : m_finalBlockConsensusBuffer[m_mediator.m_consensusID]) {
    auto runconsensus = [this, i]() {
      ProcessFinalBlockConsensusCore(i.second, MessageOffset::BODY, i.first);
    };
    DetachedFunction(1, runconsensus);
  }
}

void DirectoryService::CleanFinalblockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexFinalBlockConsensusBuffer);
  m_finalBlockConsensusBuffer.clear();
}

bool DirectoryService::ProcessFinalBlockConsensusCore(
    [[gnu::unused]] const bytes& message, [[gnu::unused]] unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (!CheckState(PROCESS_FINALBLOCKCONSENSUS)) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Ignoring consensus message. I am at state " << m_state);
    return false;
  }

  // Consensus messages must be processed in correct sequence as they come in
  // It is possible for ANNOUNCE to arrive before correct DS state
  // In that case, state transition will occurs and ANNOUNCE will be processed.
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
                          "m_consensusObject is a nullptr. It has not "
                          "been initialized.")
              return false;
            }
            return m_consensusObject->CanProcessMessage(message, offset);
          })) {
    // Correct order preserved
  } else {
    LOG_GENERAL(
        WARNING,
        "Timeout while waiting for correct order of Final Block consensus "
        "messages");
    return false;
  }

  lock_guard<mutex> g(m_mutexConsensus);

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();

  if (state == ConsensusCommon::State::DONE) {
    cv_viewChangeFinalBlock.notify_all();
    m_viewChangeCounter = 0;
    ProcessFinalBlockConsensusWhenDone();
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Oops, no consensus reached - consensus error. "
              "error number: "
                  << to_string(m_consensusObject->GetConsensusErrorCode())
                  << " error message: "
                  << (m_consensusObject->GetConsensusErrorMsg()));

    if (m_consensusObject->GetConsensusErrorCode() ==
        ConsensusCommon::FINALBLOCK_MISSING_MICROBLOCKS) {
      // Missing microblocks proposed by leader. Will attempt to fetch
      // missing microblocks from leader, set to a valid state to accept cosig1
      // and cosig2

      // Block till txn is fetched
      unique_lock<mutex> lock(m_mutexCVMissingMicroBlock);
      if (cv_MissingMicroBlock.wait_for(
              lock, chrono::seconds(FETCHING_MISSING_DATA_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "fetching missing microblocks timeout");
      } else {
        // Re-run consensus
        m_consensusObject->RecoveryAndProcessFromANewState(
            ConsensusCommon::INITIAL);

        auto rerunconsensus = [this, message, offset, from]() {
          PrepareRunConsensusOnFinalBlockNormal();
          ProcessFinalBlockConsensusCore(message, offset, from);
        };
        DetachedFunction(1, rerunconsensus);
        return true;
      }
    } else if (m_consensusObject->GetConsensusErrorCode() ==
               ConsensusCommon::MISSING_TXN) {
      // Missing txns in microblock proposed by leader. Will attempt to fetch
      // missing txns from leader, set to a valid state to accept cosig1 and
      // cosig2
      LOG_GENERAL(INFO, "Start pending for fetching missing txns")

      // Block till txn is fetched
      unique_lock<mutex> lock(m_mediator.m_node->m_mutexCVMicroBlockMissingTxn);
      if (m_mediator.m_node->cv_MicroBlockMissingTxn.wait_for(
              lock, chrono::seconds(FETCHING_MISSING_DATA_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "fetching missing txn timeout");
      } else {
        // Re-run consensus
        m_consensusObject->RecoveryAndProcessFromANewState(
            ConsensusCommon::INITIAL);

        auto reprocessconsensus = [this, message, offset, from]() {
          ProcessFinalBlockConsensusCore(message, offset, from);
        };
        DetachedFunction(1, reprocessconsensus);
        return true;
      }
    }

    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "No consensus reached. Wait for view change. ");
    return false;
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << m_consensusObject->GetStateString());
    cv_processConsensusMessage.notify_all();
  }
  return true;
}
