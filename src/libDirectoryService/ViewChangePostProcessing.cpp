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
#include "libNetwork/Guard.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;

bool DirectoryService::ComposeVCBlockForSender(bytes& vcblock_message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComposeVCBlockForSender not "
                "expected to be called from LookUp node.");
    return false;
  }

  LOG_MARKER();

  vcblock_message.clear();

  vcblock_message = {MessageType::NODE, NodeInstructionType::VCBLOCK};

  if (!Messenger::SetNodeVCBlock(vcblock_message, MessageOffset::BODY,
                                 *m_pendingVCBlock)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetNodeVCBlock failed.");
    return false;
  }
  return true;
}

void DirectoryService::CleanUpViewChange(bool isPrecheckFail) {
  LOG_MARKER();
  cv_ViewChangeVCBlock.notify_all();
  m_candidateLeaderIndex = 0;
  m_cumulativeFaultyLeaders.clear();

  if (isPrecheckFail) {
    m_viewChangeCounter = 0;
  }
}

void DirectoryService::ProcessViewChangeConsensusWhenDone() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessViewChangeConsensusWhenDone not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "View change consensus DONE");
  m_pendingVCBlock->SetCoSignatures(*m_consensusObject);

  unsigned int index = 0;
  unsigned int count = 0;

  vector<PubKey> keys;
  for (auto const& kv : *m_mediator.m_DSCommittee) {
    if (m_pendingVCBlock->GetB2().at(index)) {
      keys.emplace_back(kv.first);
      count++;
    }
    index++;
  }

  // Verify cosig against vcblock
  shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
  if (aggregatedKey == nullptr) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    return;
  }

  bytes message;
  if (!m_pendingVCBlock->GetHeader().Serialize(message, 0)) {
    LOG_GENERAL(WARNING, "VCBlockHeader serialization failed");
    return;
  }
  m_pendingVCBlock->GetCS1().Serialize(message, message.size());
  BitVector::SetBitVector(message, message.size(), m_pendingVCBlock->GetB1());
  if (!MultiSig::GetInstance().MultiSigVerify(message, 0, message.size(),
                                              m_pendingVCBlock->GetCS2(),
                                              *aggregatedKey)) {
    LOG_GENERAL(WARNING, "cosig verification fail");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return;
  }

  Peer newLeaderNetworkInfo;
  PubKey newLeaderPubKey;
  unsigned char viewChangeState;
  {
    lock_guard<mutex> g(m_mutexPendingVCBlock);

    newLeaderNetworkInfo =
        m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo();
    newLeaderPubKey = m_pendingVCBlock->GetHeader().GetCandidateLeaderPubKey();
    viewChangeState = m_pendingVCBlock->GetHeader().GetViewChangeState();
  }

  if (newLeaderNetworkInfo == m_mediator.m_selfPeer &&
      newLeaderPubKey == m_mediator.m_selfKey.second) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "After view change, I am the new DS leader!");
    m_mode = PRIMARY_DS;
  } else {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "After view change, I am ds backup");
    m_mode = BACKUP_DS;
  }

  auto tmpDSCommittee = *(m_mediator.m_DSCommittee);

  {
    lock_guard<mutex> g2(m_mediator.m_mutexDSCommittee);

    // Pushing faulty leader to back of the deque

    if (!GUARD_MODE) {
      for (const auto& faultyLeader :
           m_pendingVCBlock->GetHeader().GetFaultyLeaders()) {
        // find the faulty leader and identify the index
        DequeOfNode::iterator iterFaultyLeader;
        if (faultyLeader.second == m_mediator.m_selfPeer) {
          iterFaultyLeader = find(m_mediator.m_DSCommittee->begin(),
                                  m_mediator.m_DSCommittee->end(),
                                  make_pair(faultyLeader.first, Peer()));
        } else {
          iterFaultyLeader =
              find(m_mediator.m_DSCommittee->begin(),
                   m_mediator.m_DSCommittee->end(), faultyLeader);
        }

        // Remove faulty leader from the current ds committee structure
        // temporary
        if (iterFaultyLeader != m_mediator.m_DSCommittee->end()) {
          m_mediator.m_DSCommittee->erase(iterFaultyLeader);
        } else {
          LOG_GENERAL(WARNING, "FATAL: Cannot find "
                                   << faultyLeader.second
                                   << " to eject to back of ds committee");
        }

        // Add to the back of the ds commitee deque
        if (faultyLeader.second == m_mediator.m_selfPeer) {
          m_mediator.m_DSCommittee->emplace_back(
              make_pair(faultyLeader.first, Peer()));
        } else {
          m_mediator.m_DSCommittee->emplace_back(faultyLeader);
        }
      }
    } else {
      LOG_GENERAL(INFO, "In guard mode. Actual composition remain the same.");
    }

    // Re-calculate the new m_consensusMyID
    PairOfNode selfPubKPeerPair =
        make_pair(m_mediator.m_selfKey.second, Peer());

    DequeOfNode::iterator iterConsensusMyID =
        find(m_mediator.m_DSCommittee->begin(), m_mediator.m_DSCommittee->end(),
             selfPubKPeerPair);

    if (iterConsensusMyID != m_mediator.m_DSCommittee->end()) {
      m_consensusMyID =
          distance(m_mediator.m_DSCommittee->begin(), iterConsensusMyID);
    } else {
      LOG_GENERAL(
          WARNING,
          "FATAL: Unable to set m_consensusMyID. Cannot find myself in the ds "
          "committee");
      return;
    }

    // Update the index for the new leader
    PairOfNode candidateLeaderInfo = make_pair(
        m_pendingVCBlock->GetHeader().GetCandidateLeaderPubKey(),
        m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo());
    if (candidateLeaderInfo.first == m_mediator.m_selfKey.second &&
        candidateLeaderInfo.second == m_mediator.m_selfPeer) {
      SetConsensusLeaderID(m_consensusMyID.load());
    } else {
      DequeOfNode::iterator iterConsensusLeaderID =
          find(m_mediator.m_DSCommittee->begin(),
               m_mediator.m_DSCommittee->end(), candidateLeaderInfo);

      if (iterConsensusLeaderID != m_mediator.m_DSCommittee->end()) {
        SetConsensusLeaderID(
            distance(m_mediator.m_DSCommittee->begin(), iterConsensusLeaderID));
      } else {
        LOG_GENERAL(WARNING, "FATAL Cannot find new leader in the ds committee "
                                 << candidateLeaderInfo.second);
        return;
      }
    }

    LOG_GENERAL(INFO, "New m_consensusLeaderID " << GetConsensusLeaderID());
    LOG_GENERAL(INFO, "New view of ds committee: ");
    for (auto& i : *m_mediator.m_DSCommittee) {
      LOG_GENERAL(INFO, i.second);
    }

    // Consensus update for DS shard
    {
      lock_guard<mutex> g(m_mediator.m_node->m_mutexShardMember);
      m_mediator.m_node->m_myShardMembers = m_mediator.m_DSCommittee;
    }
    m_mediator.m_node->SetConsensusMyID(m_consensusMyID.load());
    m_mediator.m_node->SetConsensusLeaderID(GetConsensusLeaderID());
    if (m_mediator.m_node->GetConsensusMyID() ==
        m_mediator.m_node->GetConsensusLeaderID()) {
      m_mediator.m_node->m_isPrimary = true;
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "I am leader of the DS shard");
    } else {
      m_mediator.m_node->m_isPrimary = false;
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "I am backup member of the DS shard");
    }
  }

  switch (viewChangeState) {
    case DSBLOCK_CONSENSUS:
    case DSBLOCK_CONSENSUS_PREP:
      SetState(DSBLOCK_CONSENSUS_PREP);
      break;
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
      SetState(FINALBLOCK_CONSENSUS_PREP);
      break;
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
    default:
      break;
  }

  auto func = [this, viewChangeState]() -> void {
    ProcessNextConsensus(viewChangeState);
  };
  DetachedFunction(1, func);

  // Store to blockLink
  uint64_t latestInd = m_mediator.m_blocklinkchain.GetLatestIndex() + 1;
  m_mediator.m_blocklinkchain.AddBlockLink(
      latestInd, m_pendingVCBlock->GetHeader().GetViewChangeDSEpochNo(),
      BlockType::VC, m_pendingVCBlock->GetBlockHash());

  bytes dst;
  m_pendingVCBlock->Serialize(dst, 0);

  if (!BlockStorage::GetBlockStorage().PutVCBlock(
          m_pendingVCBlock->GetBlockHash(), dst)) {
    LOG_GENERAL(WARNING, "Unable to put VC Block");
    return;
  }

  SendDataToLookupFunc t_sendDataToLookupFunc = nullptr;
  // Broadcasting vcblock to lookup nodes iff view change does not occur before
  // ds block consensus. This is to be consistent with how normal node process
  // the vc block (before ds block).
  if (viewChangeState != DSBLOCK_CONSENSUS &&
      viewChangeState != DSBLOCK_CONSENSUS_PREP) {
    t_sendDataToLookupFunc = SendDataToLookupFuncDefault;
  }

  switch (viewChangeState) {
    case DSBLOCK_CONSENSUS:
    case DSBLOCK_CONSENSUS_PREP: {
      // Do not send to shard node as sharding structure is not yet formed.
      // VC block(s) will concat with ds block and sharding structure to form
      // vcdsmessage, which then will be send to shard node for processing.
      lock_guard<mutex> g(m_mutexVCBlockVector);
      m_VCBlockVector.emplace_back(*m_pendingVCBlock.get());
      break;
    }
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP: {
      break;
    }
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
    default:
      LOG_EPOCH(
          INFO, m_mediator.m_currentEpochNum,
          "illegal view change state. state: " << to_string(viewChangeState));
  }

  if (t_sendDataToLookupFunc) {
    auto composeVCBlockForSender = [this](bytes& vcblock_message) -> bool {
      return ComposeVCBlockForSender(vcblock_message);
    };

    // Acquire shard receivers cosigs from MicroBlocks
    unordered_map<uint32_t, BlockBase> t_microBlocks;
    const auto& microBlocks = m_microBlocks
        [m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()];
    for (const auto& microBlock : microBlocks) {
      t_microBlocks.emplace(microBlock.GetHeader().GetShardId(), microBlock);
    }

    DequeOfShard t_shards;
    if (m_forceMulticast && GUARD_MODE) {
      ReloadGuardedShards(t_shards);
    }

    DataSender::GetInstance().SendDataToOthers(
        *m_pendingVCBlock, tmpDSCommittee,
        t_shards.empty() ? m_shards : t_shards, t_microBlocks,
        m_mediator.m_lookup->GetLookupNodes(),
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(),
        m_consensusMyID, composeVCBlockForSender, m_forceMulticast.load(),
        t_sendDataToLookupFunc);
  }
}

void DirectoryService::ProcessNextConsensus(unsigned char viewChangeState) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessNextConsensus not expected to be "
                "called from LookUp node.");
    return;
  }

  this_thread::sleep_for(chrono::seconds(POST_VIEWCHANGE_BUFFER));

  switch (viewChangeState) {
    case DSBLOCK_CONSENSUS:
    case DSBLOCK_CONSENSUS_PREP:
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Re-running dsblock consensus");
      RunConsensusOnDSBlock();
      break;
    case FINALBLOCK_CONSENSUS:
    case FINALBLOCK_CONSENSUS_PREP:
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Re-running finalblock consensus");
      RunConsensusOnFinalBlock();
      break;
    case VIEWCHANGE_CONSENSUS:
    case VIEWCHANGE_CONSENSUS_PREP:
    default:
      LOG_EPOCH(
          INFO, m_mediator.m_currentEpochNum,
          "illegal view change state. state: " << to_string(viewChangeState));
  }
}

bool DirectoryService::ProcessViewChangeConsensus(const bytes& message,
                                                  unsigned int offset,
                                                  const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessViewChangeConsensus not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();
  // Consensus messages must be processed in correct sequence as they come in
  // It is possible for ANNOUNCE to arrive before correct DS state
  // In that case, ANNOUNCE will sleep for a second below
  // If COLLECTIVESIG also comes in, it's then possible COLLECTIVESIG will be
  // processed before ANNOUNCE! So, ANNOUNCE should acquire a lock here

  {
    lock_guard<mutex> g(m_mutexConsensus);

    if (!CheckState(PROCESS_VIEWCHANGECONSENSUS)) {
      std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeConsensusObj);
      if (cv_ViewChangeConsensusObj.wait_for(
              cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
              [this] { return CheckState(PROCESS_VIEWCHANGECONSENSUS); })) {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Successfully transit to viewchange consensus or I "
                  "am in the "
                  "correct state.");
      } else {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "Time out while waiting for state transition to view "
                  "change "
                  "consensus and "
                  "consensus object creation. Most likely view change "
                  "didn't "
                  "occur. A malicious node may be trying to initate view "
                  "change.");
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
                "Timeout while waiting for correct order of View Change "
                "Block consensus "
                "messages");
    return false;
  }

  lock_guard<mutex> g(m_mutexConsensus);

  if (!CheckState(PROCESS_VIEWCHANGECONSENSUS)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Not in PROCESS_VIEWCHANGECONSENSUS state");
    return false;
  }

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Consensus = " << m_consensusObject->GetStateString());

  if (state == ConsensusCommon::State::DONE) {
    CleanUpViewChange(false);
    ProcessViewChangeConsensusWhenDone();
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "No consensus reached. Re-attempting");
    return false;
  } else {
    cv_processConsensusMessage.notify_all();
  }
  return true;
}

// Exposing this function so that libNode can use it to check state
bool DirectoryService::IsDSBlockVCState(unsigned char vcBlockState) {
  return ((DirState)vcBlockState == DSBLOCK_CONSENSUS_PREP ||
          (DirState)vcBlockState == DSBLOCK_CONSENSUS);
}

void DirectoryService::ClearVCBlockVector() {
  lock_guard<mutex> g(m_mutexVCBlockVector);
  m_VCBlockVector.clear();
}
