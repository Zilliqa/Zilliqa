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
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
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
                << ", Timestamp: " << m_finalBlock->GetHeader().GetTimestamp()
                << ", NumTxs: " << m_finalBlock->GetHeader().GetNumTxs());

  vector<unsigned char> serializedTxBlock;
  m_finalBlock->Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(
      m_finalBlock->GetHeader().GetBlockNum(), serializedTxBlock);
}

bool DirectoryService::SendFinalBlockToLookupNodes() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendFinalBlockToLookupNodes not "
                "expected to be called from LookUp node.");
    return true;
  }

  vector<unsigned char> finalblock_message = {MessageType::NODE,
                                              NodeInstructionType::FINALBLOCK};

  const uint64_t dsBlockNumber =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  vector<unsigned char> stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetNodeFinalBlock(finalblock_message, MessageOffset::BODY, 0,
                                    dsBlockNumber, m_mediator.m_consensusID,
                                    *m_finalBlock, stateDelta)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetNodeFinalBlock failed.");
    return false;
  }

  m_mediator.m_lookup->SendMessageToLookupNodes(finalblock_message);

  return true;
}

void DirectoryService::SendFinalBlockToShardNodes(
    unsigned int my_DS_cluster_num, unsigned int my_shards_lo,
    unsigned int my_shards_hi) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendFinalBlockToShardNodes not expected "
                "to be called from LookUp node.");
    return;
  }

  // Too few target shards - avoid asking all DS clusters to send
  LOG_MARKER();

  if ((my_DS_cluster_num + 1) > m_shards.size()) {
    return;
  }

  const uint64_t dsBlockNumber =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  vector<unsigned char> stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  auto p = m_shards.begin();
  advance(p, my_shards_lo);

  for (unsigned int shardId = my_shards_lo; shardId <= my_shards_hi;
       shardId++) {
    vector<unsigned char> finalblock_message = {
        MessageType::NODE, NodeInstructionType::FINALBLOCK};

    if (!Messenger::SetNodeFinalBlock(
            finalblock_message, MessageOffset::BODY, shardId, dsBlockNumber,
            m_mediator.m_consensusID, *m_finalBlock, stateDelta)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetNodeFinalBlock failed.");
      return;
    }

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(finalblock_message);
    vector<unsigned char> this_msg_hash = sha256.Finalize();
    LOG_STATE(
        "[INFOR]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "][" << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
        << "]["
        << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
               .substr(0, 6)
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] FBBLKGEN");

    if (BROADCAST_GOSSIP_MODE) {
      // Choose N other Shard nodes to be recipient of final block
      std::vector<Peer> shardFinalBlockReceivers;
      unsigned int numOfFinalBlockReceivers = std::min(
          NUM_FINALBLOCK_GOSSIP_RECEIVERS_PER_SHARD, (uint32_t)p->size());

      for (unsigned int i = 0; i < numOfFinalBlockReceivers; i++) {
        const auto& kv = p->at(i);
        shardFinalBlockReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            " PubKey: " << DataConversion::SerializableToHexStr(
                               std::get<SHARD_NODE_PUBKEY>(kv))
                        << " IP: "
                        << std::get<SHARD_NODE_PEER>(kv).GetPrintableIPAddress()
                        << " Port: "
                        << std::get<SHARD_NODE_PEER>(kv).m_listenPortHost);
      }

      P2PComm::GetInstance().SendRumorToForeignPeers(shardFinalBlockReceivers,
                                                     finalblock_message);
    } else {
      vector<Peer> shard_peers;

      for (const auto& kv : *p) {
        shard_peers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            " PubKey: " << DataConversion::SerializableToHexStr(
                               std::get<SHARD_NODE_PUBKEY>(kv))
                        << " IP: "
                        << std::get<SHARD_NODE_PEER>(kv).GetPrintableIPAddress()
                        << " Port: "
                        << std::get<SHARD_NODE_PEER>(kv).m_listenPortHost);
      }

      P2PComm::GetInstance().SendBroadcastMessage(shard_peers,
                                                  finalblock_message);
    }

    p++;
  }
}

// void DirectoryService::StoreMicroBlocksToDisk()
// {
//     LOG_MARKER();
//     for(auto microBlock : m_microBlocks)
//     {

//         LOG_GENERAL(INFO,  "Storing Micro Block Hash: " <<
//         microBlock.GetHeader().GetTxRootHash() <<
//             " with Type: " << microBlock.GetHeader().GetType() <<
//             ", Version: " << microBlock.GetHeader().GetVersion() <<
//             ", Timestamp: " << microBlock.GetHeader().GetTimestamp() <<
//             ", NumTxs: " << microBlock.GetHeader().GetNumTxs());

//         vector<unsigned char> serializedMicroBlock;
//         microBlock.Serialize(serializedMicroBlock, 0);
//         BlockStorage::GetBlockStorage().PutMicroBlock(microBlock.GetHeader().GetTxRootHash(),
//                                                serializedMicroBlock);
//     }
//     m_microBlocks.clear();
// }

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

  m_mediator.HeartBeatPulse();

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

  bool isVacuousEpoch = m_mediator.GetIsVacuousEpoch();

  // StoreMicroBlocksToDisk();
  StoreFinalBlockToDisk();

  if (isVacuousEpoch) {
    AccountStore::GetInstance().MoveUpdatesToDisk();
    BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED, {'0'});
  } else {
    AccountStore::GetInstance().CommitTemp();
    // Coinbase
    SaveCoinbase(m_finalBlock->GetB1(), m_finalBlock->GetB2(), -1);
  }

  m_mediator.UpdateDSBlockRand();
  m_mediator.UpdateTxBlockRand();

  if (m_toSendTxnToLookup && !isVacuousEpoch) {
    m_mediator.m_node->CallActOnFinalblock();
  }

  // TODO: Refine this
  unsigned int nodeToSendToLookUpLo = COMM_SIZE / 4;
  unsigned int nodeToSendToLookUpHi =
      nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

  if (m_consensusMyID > nodeToSendToLookUpLo &&
      m_consensusMyID < nodeToSendToLookUpHi) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Part of the DS committeement (assigned) that will send the "
              "Final Block to "
              "the lookup nodes");
    SendFinalBlockToLookupNodes();
  }

  // uint8_t tx_sharing_mode
  //     = (m_sharingAssignment.size() > 0) ? DS_FORWARD_ONLY : ::IDLE;
  // m_mediator.m_node->ActOnFinalBlock(tx_sharing_mode, m_sharingAssignment);

  unsigned int my_DS_cluster_num;
  unsigned int my_shards_lo;
  unsigned int my_shards_hi;

  LOG_STATE(
      "[FLBLK]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] BEFORE SENDING FINAL BLOCK");

  DetermineShardsToSendBlockTo(my_DS_cluster_num, my_shards_lo, my_shards_hi);
  SendFinalBlockToShardNodes(my_DS_cluster_num, my_shards_lo, my_shards_hi);

  LOG_STATE(
      "[FLBLK]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] AFTER SENDING FINAL BLOCK");

  {
    lock_guard<mutex> g(m_mediator.m_mutexCurSWInfo);
    if (0 == (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW) &&
        m_mediator.m_curSWInfo.GetUpgradeDS() ==
            ((m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW) +
             INIT_DS_EPOCH_NUM)) {
      auto func = [this]() mutable -> void {
        UpgradeManager::GetInstance().ReplaceNode(m_mediator);
      };
      DetachedFunction(1, func);
    }
  }

  AccountStore::GetInstance().InitTemp();
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
      m_dsStartedMicroblockConsensus = false;
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "[No PoW needed] Waiting for Microblock.");

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

        auto func2 = [this]() mutable -> void {
          if (!m_dsStartedMicroblockConsensus) {
            m_dsStartedMicroblockConsensus = true;
            m_mediator.m_node->RunConsensusOnMicroBlock();
          }
        };

        DetachedFunction(1, func2);

        std::unique_lock<std::mutex> cv_lk(m_MutexScheduleFinalBlockConsensus);
        if (cv_scheduleFinalBlockConsensus.wait_for(
                cv_lk,
                std::chrono::seconds(DS_MICROBLOCK_CONSENSUS_OBJECT_TIMEOUT)) ==
            std::cv_status::timeout) {
          LOG_GENERAL(WARNING,
                      "Timeout: Didn't finish DS Microblock. Proceeds "
                      "without it");

          RunConsensusOnFinalBlock(true);
        }
      }
    }
  };

  DetachedFunction(1, func);
}

bool DirectoryService::ProcessFinalBlockConsensus(
    const vector<unsigned char>& message, unsigned int offset,
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
      if (!m_dsStartedMicroblockConsensus) {
        m_dsStartedMicroblockConsensus = true;
      }
      cv_scheduleFinalBlockConsensus.notify_all();
      RunConsensusOnFinalBlock(true);
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
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from) {
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
              "Oops,     - what to do now???");

    if (m_consensusObject->GetConsensusErrorCode() ==
        ConsensusCommon::FINALBLOCK_MISSING_MICROBLOCKS) {
      // Missing microblocks proposed by leader. Will attempt to fetch
      // missing microblocks from leader, set to a valid state to accept cosig1
      // and cosig2
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << (m_consensusObject->GetConsensusErrorMsg()));

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
          if (m_mediator.GetIsVacuousEpoch()) {
            AccountStore::GetInstance().RevertCommitTemp();
            AccountStore::GetInstance().CommitTempReversible();
          }

          ProcessFinalBlockConsensusCore(message, offset, from);
        };
        DetachedFunction(1, rerunconsensus);
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
