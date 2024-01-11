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
#include "libData/AccountStore/AccountStore.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libNode/Node.h"
#include "libPersistence/ContractStorage.h"
#include "libScilla/ScillaClient.h"
#include "libUtils/CommonUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

namespace zil {
namespace local {

class FinalBlockPostProcessingVariables {
  int mbInFinal = 0;

 public:
  std::unique_ptr<Z_I64GAUGE> temp;

  void SetMbInFinal(int count) {
    Init();
    mbInFinal = count;
  }

  void Init() {
    if (!temp) {
      temp = std::make_unique<Z_I64GAUGE>(
          Z_FL::BLOCKS, "finalblockpostproc.gauge",
          "Final block post processing state", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(mbInFinal, {{"counter", "MbInFinal"}});
      });
    }
  }
};

static FinalBlockPostProcessingVariables variables{};

}  // namespace local
}  // namespace zil

bool DirectoryService::StoreFinalBlockToDisk() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::StoreFinalBlockToDisk not expected to "
                "be called from LookUp node.");
    return true;
  }

  if (m_mediator.m_node->m_microblock != nullptr &&
      m_mediator.m_node->m_microblock->GetHeader().GetTxRootHash() !=
          TxnHash()) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Storing DS MicroBlock" << endl
                                      << *(m_mediator.m_node->m_microblock));
    zbytes body;
    m_mediator.m_node->m_microblock->Serialize(body, 0);
    if (!BlockStorage::GetBlockStorage().PutMicroBlock(
            m_mediator.m_node->m_microblock->GetBlockHash(),
            m_mediator.m_node->m_microblock->GetHeader().GetEpochNum(),
            m_mediator.m_node->m_microblock->GetHeader().GetShardId(), body)) {
      LOG_GENERAL(WARNING, "Failed to put microblock in persistence");
      return false;
    }
  }

  // Add finalblock to txblockchain
  m_mediator.m_node->AddBlock(*m_finalBlock);

  // To make sure pow submission is accepted. But it is not verified until state
  // switches to POW_SUBMISSION
  if (m_mediator.GetIsVacuousEpoch()) {
    m_powSubmissionWindowExpired = false;
  }

  m_mediator.IncreaseEpochNum();

  // At this point, the transactions in the last Epoch is no longer useful, thus
  // erase.
  // m_mediator.m_node->EraseCommittedTransactions(m_mediator.m_currentEpochNum
  //                                               - 2);

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Storing Tx Block" << endl
                               << *m_finalBlock);

  zil::local::variables.SetMbInFinal(m_finalBlock->GetMicroBlockInfos().size());
  zbytes serializedTxBlock;
  m_finalBlock->Serialize(serializedTxBlock, 0);
  if (!BlockStorage::GetBlockStorage().PutTxBlock(m_finalBlock->GetHeader(),
                                                  serializedTxBlock)) {
    LOG_GENERAL(WARNING, "Failed to put microblock in persistence");
    return false;
  }

  zbytes stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);
  if (!BlockStorage::GetBlockStorage().PutStateDelta(
          m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
          stateDelta)) {
    LOG_GENERAL(WARNING, "Failed to put statedelta in persistence");
    return false;
  }
  return true;
}

bool DirectoryService::ComposeFinalBlockMessageForSender(
    zbytes& finalblock_message) {
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

  zbytes stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetNodeFinalBlock(finalblock_message, MessageOffset::BODY,
                                    dsBlockNumber, m_mediator.m_consensusID,
                                    *m_finalBlock, stateDelta)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
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

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Final block consensus DONE, committee size: "
                << m_mediator.m_DSCommittee->size()
                << ", shard size: " << std::size(m_shards));

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
  m_finalBlock->SetCoSignatures(ConsensusObjectToCoSig(*m_consensusObject));

  // Update the DS microblock with the same co-signatures from the consensus
  // If we don't do this, DataSender won't be able to process it
  m_mediator.m_node->m_microblock->SetCoSignatures(
      ConsensusObjectToCoSig(*m_consensusObject));

  bool isVacuousEpoch = m_mediator.GetIsVacuousEpoch();

  if (m_mediator.m_node->m_microblock != nullptr && !isVacuousEpoch) {
    m_mediator.m_node->UpdateProcessedTransactions();
  }

  // StoreMicroBlocksToDisk();

  auto resumeBlackList = []() mutable -> void {
    this_thread::sleep_for(chrono::seconds(RESUME_BLACKLIST_DELAY_IN_SECONDS));
    Blacklist::GetInstance().Enable(true);
  };

  DetachedFunction(1, resumeBlackList);

  if (!StoreFinalBlockToDisk()) {
    LOG_GENERAL(WARNING, "StoreFinalBlockToDisk failed!");
    return;
  }

  if (isVacuousEpoch) {
    // Restart scilla client after every vacuous epoch
    ScillaClient::GetInstance().RestartScillaClient();

    auto writeStateToDisk = [this]() -> void {
      if (!AccountStore::GetInstance().MoveUpdatesToDisk(
              m_mediator.m_dsBlockChain.GetLastBlock()
                  .GetHeader()
                  .GetBlockNum())) {
        LOG_GENERAL(WARNING, "MoveUpdatesToDisk() failed, what to do?");
        return;
      } else {
        if (!BlockStorage::GetBlockStorage().PutLatestEpochStatesUpdated(
                m_mediator.m_currentEpochNum)) {
          LOG_GENERAL(WARNING, "BlockStorage::PutLatestEpochStatesUpdated "
                                   << m_mediator.m_currentEpochNum
                                   << " failed");
          return;
        }
        if (!BlockStorage::GetBlockStorage().PutEpochFin(
                m_mediator.m_currentEpochNum)) {
          LOG_GENERAL(WARNING, "BlockStorage::PutEpochFin failed "
                                   << m_mediator.m_currentEpochNum);
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
      }
      if (ENABLE_ACCOUNTS_POPULATING &&
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() <
              PREGEN_ACCOUNT_TIMES) {
        m_mediator.m_node->PopulateAccounts();
      }
    };
    DetachedFunction(1, writeStateToDisk);

  } else {
    // Coinbase
    SaveCoinbase(m_finalBlock->GetB1(), m_finalBlock->GetB2(),
                 CoinbaseReward::FINALBLOCK_REWARD,
                 m_mediator.m_currentEpochNum);
    m_totalTxnFees += m_finalBlock->GetHeader().GetRewards();

    if (!BlockStorage::GetBlockStorage().PutEpochFin(
            m_mediator.m_currentEpochNum)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutEpochFin failed "
                               << m_mediator.m_currentEpochNum);
      return;
    }
  }

  // Clear STL memory cache
  DetachedFunction(1, CommonUtils::ReleaseSTLMemoryCache);

  m_mediator.UpdateDSBlockRand();
  m_mediator.UpdateTxBlockRand();

  auto composeFinalBlockMessageForSender = [this](zbytes& message) -> bool {
    return ComposeFinalBlockMessageForSender(message);
  };

  // Acquire shard receivers cosigs from MicroBlocks
  unordered_map<uint32_t, BlockBase> t_microBlocks;
  const auto& microBlocks = m_microBlocks
      [m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()];
  for (const auto& microBlock : microBlocks) {
    t_microBlocks.emplace(microBlock.GetHeader().GetShardId(), microBlock);
  }

  DequeOfShardMembers t_shards;
  if (m_forceMulticast && GUARD_MODE) {
    ReloadGuardedShards(t_shards);
  }

  LOG_GENERAL(INFO,
              "Consensus is done, sending final block to others, ds_state: "
                  << GetStateString());
  DataSender::GetInstance().SendDataToOthers(
      *m_finalBlock, *m_mediator.m_DSCommittee,
      t_shards.empty() ? m_shards : t_shards, t_microBlocks,
      m_mediator.m_lookup->GetLookupNodes(),
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(), m_consensusMyID,
      composeFinalBlockMessageForSender, m_forceMulticast.load());

  LOG_STATE(
      "[FLBLK]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] AFTER SENDING FLBLK");

  const bool& toSendPendingTxn = !(m_mediator.m_node->IsUnconfirmedTxnEmpty());

  if ((m_mediator.m_node->m_microblock != nullptr &&
       m_mediator.m_node->m_microblock->GetHeader().GetTxRootHash() !=
           TxnHash())) {
    m_mediator.m_node->CallActOnFinalblock();
  }

  if (toSendPendingTxn) {
    m_mediator.m_node->SendPendingTxnToLookup();
  }
  m_mediator.m_node->ClearUnconfirmedTxn();

  AccountStore::GetInstance().InitTemp();
  AccountStore::GetInstance().InitRevertibles();
  m_stateDeltaFromShards.clear();

  m_allPoWConns.clear();
  ClearDSPoWSolns();
  ResetPoWSubmissionCounter();
  if (isVacuousEpoch) {
    SetState(POW_SUBMISSION);
  }

  auto func = [this, isVacuousEpoch]() mutable -> void {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "START OF a new EPOCH");
    if (isVacuousEpoch) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "[PoW needed]");

      StartNewDSEpochConsensus();
    } else {
      m_mediator.m_node->UpdateStateForNextConsensusRound();
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "[No PoW needed] Waiting for Microblock.");

      if (m_mediator.m_node->m_myshardId == DEFAULT_SHARD_ID ||
          m_dsEpochAfterUpgrade) {
        LOG_GENERAL(INFO,
                    "[No PoW needed] No other shards. So no other microblocks "
                    "expected to be received");

        auto func1 = [this]() mutable -> void {
          m_mediator.m_node->CommitTxnPacketBuffer();
        };
        DetachedFunction(1, func1);
        SetState(FINALBLOCK_CONSENSUS_PREP);
        RunConsensusOnFinalBlock();
      }
    }
  };
  DetachedFunction(1, func);
}

bool DirectoryService::ProcessFinalBlockConsensus(
    const zbytes& message, unsigned int offset, const Peer& from,
    const unsigned char& startByte,
    std::shared_ptr<zil::p2p::P2PServerConnection>) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessFinalBlockConsensus not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_GENERAL(
      INFO,
      "DirectoryService::ProcessFinalBlockConsensus() enter, ds_state is: "
          << GetStateString());

  uint32_t consensus_id = 0;
  zbytes reserialized_message;
  PubKey senderPubKey;

  if (!m_consensusObject) {
    LOG_GENERAL(WARNING,
                "Consensus object has not been created yet! Please check "
                "consensus timings!");
    return false;
  }

  if (!m_consensusObject->PreProcessMessage(
          message, offset, consensus_id, senderPubKey, reserialized_message)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "PreProcessMessage failed");
    return false;
  }

  if (!CheckIfDSNode(senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "ProcessFinalBlockConsensus signed by non ds member");
    return false;
  }

  if (!CheckState(PROCESS_FINALBLOCKCONSENSUS)) {
    // don't buffer the Final block consensus message if i am non-ds node
    if (m_mode != Mode::BACKUP_DS) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Ignoring final block consensus message from wrong timing if "
                "not backup");
      return false;
    }
    // Only buffer the Final block consensus message if in the immediate states
    // before consensus, or when doing view change
    if ((m_state != FINALBLOCK_CONSENSUS_PREP) &&
        (m_state != VIEWCHANGE_CONSENSUS)) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Ignoring final block consensus message");
      return false;
    }

    LOG_GENERAL(INFO,
                "Adding message to FinalBlockConsensusBuffer, "
                "PROCESS_FINALBLOCKCONSENSUS action is allowed in my state");
    AddToFinalBlockConsensusBuffer(consensus_id, reserialized_message, offset,
                                   from, senderPubKey);

    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Process final block arrived early, saved to buffer");

    if (consensus_id == m_mediator.m_consensusID &&
        senderPubKey ==
            m_mediator.m_DSCommittee->at(GetConsensusLeaderID()).first) {
      lock_guard<mutex> g(m_mutexPrepareRunFinalblockConsensus);
      LOG_GENERAL(INFO,
                  "DirectoryService::ProcessFinalBlockConsensus(): I'm calling "
                  "RunConsensusOnFinalBlock, ds_state is: "
                      << GetStateString());
      RunConsensusOnFinalBlock();
    }
  } else {
    if (consensus_id < m_mediator.m_consensusID) {
      LOG_GENERAL(WARNING, "Consensus ID in message ("
                               << consensus_id << ") is smaller than current ("
                               << m_mediator.m_consensusID << ")");
      return false;
    } else if (consensus_id > m_mediator.m_consensusID) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Buffer final block with larger consensus ID ("
                    << consensus_id << "), current ("
                    << m_mediator.m_consensusID << ")");
      AddToFinalBlockConsensusBuffer(consensus_id, reserialized_message, offset,
                                     from, senderPubKey);
    } else {
      LOG_GENERAL(INFO, "Calling ProcessFinalBlockConsensusCore with ds_state: "
                            << GetStateString());
      return ProcessFinalBlockConsensusCore(reserialized_message, offset, from,
                                            startByte, consensus_id);
    }
  }

  return true;
}

void DirectoryService::CommitFinalBlockConsensusBuffer() {
  LOG_MARKER();
  lock_guard<mutex> g(m_mutexFinalBlockConsensusBuffer);

  for (const auto& i : m_finalBlockConsensusBuffer[m_mediator.m_consensusID]) {
    auto runconsensus = [this, i]() {
      ProcessFinalBlockConsensusCore(
          std::get<NODE_MSG>(i), MessageOffset::BODY, std::get<NODE_PEER>(i),
          zil::p2p::START_BYTE_NORMAL, std::get<CONSENSUS_ID>(i));
    };
    DetachedFunction(1, runconsensus);
  }
}

void DirectoryService::AddToFinalBlockConsensusBuffer(
    uint32_t consensusId, const zbytes& message, unsigned int offset,
    const Peer& peer, const PubKey& senderPubKey) {
  if (message.size() <= offset) {
    LOG_GENERAL(WARNING, "The message size " << message.size()
                                             << " is less than the offset "
                                             << offset);
    return;
  }
  lock_guard<mutex> h(m_mutexFinalBlockConsensusBuffer);
  auto& vecNodeMsg = m_finalBlockConsensusBuffer[consensusId];
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
            << " already send final block consensus message for consensus id "
            << consensusId << " message type "
            << std::to_string(consensusMsgType));
    return;
  }

  vecNodeMsg.push_back(make_tuple(senderPubKey, peer, message, consensusId));
}

void DirectoryService::CleanFinalBlockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexFinalBlockConsensusBuffer);
  m_finalBlockConsensusBuffer.clear();
}

bool DirectoryService::ProcessFinalBlockConsensusCore(
    [[gnu::unused]] const zbytes& message, [[gnu::unused]] unsigned int offset,
    const Peer& from, const unsigned char& startByte, uint32_t consensusId) {
  LOG_MARKER();

  if (!CheckState(PROCESS_FINALBLOCKCONSENSUS)) {
    return false;
  }

  // Consensus messages must be processed in correct sequence as they come in
  // It is possible for ANNOUNCE to arrive before correct DS state
  // In that case, state transition will occurs and ANNOUNCE will be processed.
  std::unique_lock<mutex> cv_lk(m_mutexProcessConsensusMessage);
  // TODO: cv fix
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

  if (!CheckState(PROCESS_FINALBLOCKCONSENSUS)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Not in PROCESS_FINALBLOCKCONSENSUS state");
    return false;
  }

#ifdef VC_TEST_FB_SUSPEND_RESPONSE
  ConsensusCommon::State checkState = m_consensusObject->GetState();

  if (checkState == ConsensusCommon::State::FINALCHALLENGE_DONE &&
      m_mode == PRIMARY_DS && m_viewChangeCounter == 0 &&
      m_mediator.m_txBlockChain.GetBlockCount() % NUM_FINAL_BLOCK_PER_POW !=
          0) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "I am suspending myself to test viewchange "
              "(VC_TEST_FB_SUSPEND_RESPONSE)");
    return false;
  }
#endif  // VC_TEST_FB_SUSPEND_RESPONSE

  if (consensusId < m_mediator.m_consensusID) {
    LOG_GENERAL(WARNING, "Dropping outdated consensus message!");
    return false;
  }

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();

  if (state == ConsensusCommon::State::DONE) {
    cv_viewChangeFinalBlock.notify_all();
    m_viewChangeCounter = 0;
    ProcessFinalBlockConsensusWhenDone();
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
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

      // Block till microblock is fetched
      unique_lock<mutex> lock(m_mutexCVMissingMicroBlock);
      // TODO: cv fix
      if (cv_MissingMicroBlock.wait_for(
              lock, chrono::seconds(FETCHING_MISSING_DATA_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "fetching missing microblocks timeout");
      } else {
        // Re-run consensus
        m_consensusObject->RecoveryAndProcessFromANewState(
            ConsensusCommon::INITIAL);

        auto rerunconsensus = [this, message, offset, from, startByte,
                               consensusId]() {
          RemoveDSMicroBlock();  // Remove DS microblock from my list of
                                 // microblocks
          PrepareRunConsensusOnFinalBlockNormal();
          ProcessFinalBlockConsensusCore(message, offset, from, startByte,
                                         consensusId);
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
      // TODO: cv fix
      if (m_mediator.m_node->cv_MicroBlockMissingTxn.wait_for(
              lock, chrono::seconds(FETCHING_MISSING_DATA_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "fetching missing txn timeout");
      } else {
        // Re-run consensus
        m_consensusObject->RecoveryAndProcessFromANewState(
            ConsensusCommon::INITIAL);

        auto reprocessconsensus = [this, message, offset, from, startByte,
                                   consensusId]() {
          RemoveDSMicroBlock();  // Remove DS microblock from my list of
                                 // microblocks
          ProcessFinalBlockConsensusCore(message, offset, from, startByte,
                                         consensusId);
        };
        DetachedFunction(1, reprocessconsensus);
        return true;
      }
    }

    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "No consensus reached. Wait for view change. ");
    return false;
  } else {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Consensus = " << m_consensusObject->GetStateString());
    cv_processConsensusMessage.notify_all();
  }
  return true;
}
