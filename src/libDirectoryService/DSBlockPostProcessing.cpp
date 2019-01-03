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
#include <iterator>
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
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

void DirectoryService::StoreDSBlockToStorage() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::StoreDSBlockToStorage not expected to "
                "be called from LookUp node.");
    return;
  }

  LOG_MARKER();
  lock_guard<mutex> g(m_mutexPendingDSBlock);
  int result = m_mediator.m_dsBlockChain.AddBlock(*m_pendingDSBlock);
  LOG_EPOCH(
      INFO, std::to_string(m_mediator.m_currentEpochNum).c_str(),
      "Storing DS Block Number: "
          << m_pendingDSBlock->GetHeader().GetBlockNum()
          << ", DS PoW Difficulty: "
          << std::to_string(m_pendingDSBlock->GetHeader().GetDSDifficulty())
          << ", Difficulty: "
          << std::to_string(m_pendingDSBlock->GetHeader().GetDifficulty())
          << ", Timestamp: " << m_pendingDSBlock->GetTimestamp());

  if (result == -1) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "We failed to add pendingdsblock to dsblockchain.");
    // throw exception();
  }

  // Store DS Block to disk
  bytes serializedDSBlock;
  m_pendingDSBlock->Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(
      m_pendingDSBlock->GetHeader().GetBlockNum(), serializedDSBlock);
  m_latestActiveDSBlockNum = m_pendingDSBlock->GetHeader().GetBlockNum();
  BlockStorage::GetBlockStorage().PutMetadata(
      LATESTACTIVEDSBLOCKNUM,
      DataConversion::StringToCharArray(to_string(m_latestActiveDSBlockNum)));

  // Store to blocklink
  uint64_t latestInd = m_mediator.m_blocklinkchain.GetLatestIndex() + 1;

  m_mediator.m_blocklinkchain.AddBlockLink(
      latestInd, m_pendingDSBlock->GetHeader().GetBlockNum(), BlockType::DS,
      m_pendingDSBlock->GetBlockHash());
}

bool DirectoryService::ComposeDSBlockMessageForSender(bytes& dsblock_message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComposeDSBlockMessageForSender not expected "
                "to be called from LookUp node.");
    return false;
  }

  dsblock_message.clear();
  dsblock_message = {MessageType::NODE, NodeInstructionType::DSBLOCK};
  if (!Messenger::SetNodeVCDSBlocksMessage(dsblock_message, MessageOffset::BODY,
                                           0, *m_pendingDSBlock,
                                           m_VCBlockVector, m_shards)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetNodeVCDSBlocksMessage failed.");
    return false;
  }

  return true;
}

void DirectoryService::SendDSBlockToLookupNodesAndNewDSMembers(
    const bytes& dsblock_message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendDSBlockToLookupNodesAndNewDSMembers not "
                "expected "
                "to be called from LookUp node.");
    return;
  }
  LOG_MARKER();

  m_mediator.m_lookup->SendMessageToLookupNodes(dsblock_message);

  vector<Peer> newDSmembers;
  for (const auto& newDSmember :
       m_pendingDSBlock->GetHeader().GetDSPoWWinners()) {
    newDSmembers.emplace_back(newDSmember.second);
  }

  P2PComm::GetInstance().SendMessage(newDSmembers, dsblock_message);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I the part of the subset of the DS committee that have sent the "
            "DSBlock to the lookup nodes and newly joined ds nodes");
}

void DirectoryService::SendDSBlockToShardNodes(
    [[gnu::unused]] const bytes& dsblock_message, const DequeOfShard& shards,
    const unsigned int& my_shards_lo, const unsigned int& my_shards_hi) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendDSBlockToShardNodes not expected to "
                "be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  auto p = shards.begin();
  advance(p, my_shards_lo);

  for (unsigned int i = my_shards_lo; i < my_shards_hi; i++) {
    // Get the shard ID from the leader's info in m_publicKeyToshardIdMap
    uint32_t shardId =
        m_publicKeyToshardIdMap.at(std::get<SHARD_NODE_PUBKEY>(p->front()));

    bytes dsblock_message_to_shard = {MessageType::NODE,
                                      NodeInstructionType::DSBLOCK};
    if (!Messenger::SetNodeVCDSBlocksMessage(
            dsblock_message_to_shard, MessageOffset::BODY, shardId,
            *m_pendingDSBlock, m_VCBlockVector, m_shards)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetNodeVCDSBlocksMessage failed.");
      continue;
    }

    // Send the message
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(dsblock_message_to_shard);
    auto this_msg_hash = sha256.Finalize();

    if (BROADCAST_TREEBASED_CLUSTER_MODE) {
      // Choose N other Shard nodes to be recipient of DS block
      std::vector<Peer> shardDSBlockReceivers;
      unsigned int numOfDSBlockReceivers =
          NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD;
      if (numOfDSBlockReceivers <= NUM_DS_ELECTION) {
        LOG_GENERAL(WARNING,
                    "Adjusting NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD to be "
                    "greater than NUM_DS_ELECTION. Why not correct the "
                    "constant.xml next time.");
        numOfDSBlockReceivers = NUM_DS_ELECTION + 1;
      }
      LOG_GENERAL(
          INFO,
          "Sending message with hash: ["
              << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
              << "] to NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD:"
              << numOfDSBlockReceivers << " shard peers");

      numOfDSBlockReceivers =
          std::min(numOfDSBlockReceivers, (uint32_t)p->size());

      for (unsigned int i = 0; i < numOfDSBlockReceivers; i++) {
        shardDSBlockReceivers.emplace_back(std::get<SHARD_NODE_PEER>(p->at(i)));
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            " PubKey: "
                << DataConversion::SerializableToHexStr(
                       std::get<SHARD_NODE_PUBKEY>(p->at(i)))
                << " IP: "
                << std::get<SHARD_NODE_PEER>(p->at(i)).GetPrintableIPAddress()
                << " Port: "
                << std::get<SHARD_NODE_PEER>(p->at(i)).m_listenPortHost);
      }

      P2PComm::GetInstance().SendBroadcastMessage(shardDSBlockReceivers,
                                                  dsblock_message_to_shard);
    } else {
      vector<Peer> shard_peers;
      for (const auto& kv : *p) {
        shard_peers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
      }
      P2PComm::GetInstance().SendBroadcastMessage(shard_peers,
                                                  dsblock_message_to_shard);
    }

    p++;
  }
}

void DirectoryService::UpdateMyDSModeAndConsensusId() {
  LOG_MARKER();
  uint16_t numOfIncomingDs = m_mediator.m_dsBlockChain.GetLastBlock()
                                 .GetHeader()
                                 .GetDSPoWWinners()
                                 .size();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::UpdateMyDSModeAndConsensusId not "
                "expected to be called from LookUp node.");
    return;
  }

  uint16_t lastBlockHash = 0;
  if (m_mediator.m_currentEpochNum > 1) {
    lastBlockHash =
        DataConversion::charArrTo16Bits(m_mediator.m_dsBlockChain.GetLastBlock()
                                            .GetHeader()
                                            .GetHashForRandom()
                                            .asBytes());
  }
  // Check if I am the oldest backup DS (I will no longer be part of the DS
  // committee)
  if ((uint32_t)(m_consensusMyID + numOfIncomingDs) >=
      m_mediator.m_DSCommittee->size()) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am among the oldest backup DS -> I am now just a shard node"
                  << "\n"
                  << DS_KICKOUT_MSG);
    m_mode = IDLE;

    LOG_STATE("[IDENT][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][      ] IDLE");
  } else {
    if (!GUARD_MODE) {
      m_consensusMyID += numOfIncomingDs;
      m_consensusLeaderID = lastBlockHash % (m_mediator.m_DSCommittee->size());
      LOG_GENERAL(INFO, "No DS Guard enabled. m_consensusLeaderID "
                            << m_consensusLeaderID);
    } else {
      // DS guards index do not change
      if (m_consensusMyID >= Guard::GetInstance().GetNumOfDSGuard()) {
        m_consensusMyID += numOfIncomingDs;
        LOG_GENERAL(INFO,
                    "Not a DS Guard. m_consensusMyID: " << m_consensusMyID);
      } else {
        LOG_GENERAL(INFO, "DS Guard. m_consensusMyID: " << m_consensusMyID);
      }
      // Only DS guard can be ds leader
      m_consensusLeaderID =
          lastBlockHash % Guard::GetInstance().GetNumOfDSGuard();
      LOG_GENERAL(INFO, "DS Guard enabled. m_consensusLeaderID "
                            << m_consensusLeaderID);
    }

    if (m_mediator.m_DSCommittee->at(m_consensusLeaderID).first ==
        m_mediator.m_selfKey.second) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "I am now DS leader for the next round");
      LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                    DS_LEADER_MSG);
      LOG_STATE("[IDENT][" << std::setw(15) << std::left
                           << m_mediator.m_selfPeer.GetPrintableIPAddress()
                           << "][" << m_mediator.m_currentEpochNum << "] DSLD");
      m_mode = PRIMARY_DS;
    } else {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "I am now DS backup for the next round");
      LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                    DS_BACKUP_MSG);

      LOG_STATE("[IDENT][" << std::setw(15) << std::left
                           << m_mediator.m_selfPeer.GetPrintableIPAddress()
                           << "][" << m_mediator.m_currentEpochNum << "] DSBK");
      m_mode = BACKUP_DS;
    }
  }
}

void DirectoryService::UpdateDSCommiteeComposition() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::UpdateDSCommiteeComposition not "
                "expected to be called from LookUp node.");
    return;
  }

  // Update the DS committee composition
  LOG_MARKER();

  const map<PubKey, Peer> NewDSMembers =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSPoWWinners();
  deque<pair<PubKey, Peer>>::iterator it;

  for (const auto& DSPowWinner : NewDSMembers) {
    m_allPoWConns.erase(DSPowWinner.first);
    if (m_mediator.m_selfKey.second == DSPowWinner.first) {
      if (!GUARD_MODE) {
        m_mediator.m_DSCommittee->emplace_front(m_mediator.m_selfKey.second,
                                                Peer());
      } else {
        it = m_mediator.m_DSCommittee->begin() +
             (Guard::GetInstance().GetNumOfDSGuard());
        m_mediator.m_DSCommittee->emplace(it, m_mediator.m_selfKey.second,
                                          Peer());
      }
    } else {
      if (!GUARD_MODE) {
        m_mediator.m_DSCommittee->emplace_front(DSPowWinner);
      } else {
        it = m_mediator.m_DSCommittee->begin() +
             (Guard::GetInstance().GetNumOfDSGuard());
        m_mediator.m_DSCommittee->emplace(it, DSPowWinner);
      }
    }
    m_mediator.m_DSCommittee->pop_back();
  }
}

void DirectoryService::StartFirstTxEpoch() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::StartFirstTxEpoch not expected to be "
                "called from LookUp node.");
    return;
  }

  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexAllPOW);
    m_allPoWs.clear();
  }

  Blacklist::GetInstance().Clear();

  ClearDSPoWSolns();
  ResetPoWSubmissionCounter();
  m_viewChangeCounter = 0;

  {
    std::lock_guard<mutex> lock(m_mutexMicroBlocks);
    m_microBlocks.clear();
    m_missingMicroBlocks.clear();
    m_microBlockStateDeltas.clear();
    m_totalTxnFees = 0;
  }

  if (m_mode != IDLE) {
    lock_guard<mutex> g(m_mediator.m_node->m_mutexShardMember);
    m_mediator.m_node->m_myShardMembers = m_mediator.m_DSCommittee;

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              " DS Sharding structure: ");

    unsigned int index = 0;
    for (const auto& i : *m_mediator.m_node->m_myShardMembers) {
      if (i.second == Peer()) {
        LOG_GENERAL(INFO, "m_consensusMyID = " << index);
        m_mediator.m_node->m_consensusMyID = index;
      }

      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                " PubKey: " << DataConversion::SerializableToHexStr(i.first)
                            << " IP: " << i.second.GetPrintableIPAddress()
                            << " Port: " << i.second.m_listenPortHost);

      index++;
    }

    m_mediator.m_node->ResetConsensusId();

    // Check if I am the leader or backup of the shard
    m_mediator.m_node->m_consensusLeaderID = m_consensusLeaderID.load();

    if (m_mediator.m_node->m_consensusMyID ==
        m_mediator.m_node->m_consensusLeaderID) {
      m_mediator.m_node->m_isPrimary = true;
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "I am leader of the DS shard");
    } else {
      m_mediator.m_node->m_isPrimary = false;

      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "I am backup member of the DS shard");
    }

    // m_mediator.m_node->m_myshardId = std::numeric_limits<uint32_t>::max();
    m_mediator.m_node->m_myshardId = m_shards.size();
    m_mediator.m_node->m_justDidFallback = false;
    m_mediator.m_node->CommitTxnPacketBuffer();
    m_stateDeltaFromShards.clear();

    // Start sharding work
    SetState(MICROBLOCK_SUBMISSION);

    LOG_STATE(
        "[MIBLKSWAIT]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] BEGIN");

    m_stopRecvNewMBSubmission = false;

    if (BROADCAST_GOSSIP_MODE) {
      std::vector<std::pair<PubKey, Peer>> peers;
      std::vector<PubKey> pubKeys;
      GetEntireNetworkPeerInfo(peers, pubKeys);

      // ReInitialize RumorManager for this epoch.
      P2PComm::GetInstance().InitializeRumorManager(peers, pubKeys);
    }

    auto func = [this]() mutable -> void {
      // Check for state change. If it get stuck at microblock submission for
      // too long, move on to finalblock without the microblock
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
    };
    DetachedFunction(1, func);
  } else {
    // The oldest DS committee member will be a shard node at this point -> need
    // to set myself up as a shard node

    // I need to know my shard ID -> iterate through m_shards
    bool found = false;
    for (unsigned int i = 0; i < m_shards.size() && !found; i++) {
      for (const auto& shardNode : m_shards.at(i)) {
        if (std::get<SHARD_NODE_PUBKEY>(shardNode) ==
            m_mediator.m_selfKey.second) {
          m_mediator.m_node->SetMyshardId(i);
          found = true;
          break;
        }
      }
    }

    if (!found) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "WARNING: Oldest DS node not in any of the new shards!");
      return;
    }

    // Process sharding structure as a shard node
    if (!m_mediator.m_node->LoadShardingStructure()) {
      return;
    }

    // Finally, start as a shard node
    m_mediator.m_node->StartFirstTxEpoch();
  }
}

void DirectoryService::ProcessDSBlockConsensusWhenDone(
    [[gnu::unused]] const bytes& message, [[gnu::unused]] unsigned int offset) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessDSBlockConsensusWhenDone not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DS block consensus is DONE!!!");

  lock_guard<mutex> g(m_mediator.m_node->m_mutexDSBlock);

  if (m_mode == PRIMARY_DS) {
    LOG_STATE(
        "[DSCON]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] DONE");
  }

  {
    lock_guard<mutex> g(m_mutexPendingDSBlock);

    if (m_pendingDSBlock == nullptr) {
      LOG_GENERAL(FATAL, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
    }

    // Update the DS Block with the co-signatures from the consensus
    m_pendingDSBlock->SetCoSignatures(*m_consensusObject);

    if (m_pendingDSBlock->GetHeader().GetBlockNum() >
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
            1) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "We are missing some blocks. What to do here?");
    }
  }

  // Add the DS block to the chain
  StoreDSBlockToStorage();

  m_mediator.m_node->m_proposedGasPrice =
      max(m_mediator.m_node->m_proposedGasPrice,
          m_pendingDSBlock->GetHeader().GetGasPrice());

  m_mediator.UpdateDSBlockRand();

  // Now we can update the sharding structure and transaction sharing
  // assignments
  if (m_mode == BACKUP_DS) {
    m_shards = std::move(m_tempShards);
    m_publicKeyToshardIdMap = std::move(m_tempPublicKeyToshardIdMap);
    m_mapNodeReputation = std::move(m_tempMapNodeReputation);
  } else if (m_mode == PRIMARY_DS) {
    ClearReputationOfNodeFailToJoin(m_shards, m_mapNodeReputation);
  }

  m_mediator.m_node->m_myshardId = m_shards.size();
  BlockStorage::GetBlockStorage().PutShardStructure(
      m_shards, m_mediator.m_node->m_myshardId);

  {
    // USe mutex during the composition and sending of vcds block message
    lock_guard<mutex> g(m_mutexVCBlockVector);

    // Before sending ds block to lookup/other shard-nodes and starting my 1st
    // txn epoch from this ds epoch, lets give enough time for all other ds
    // nodes to receive DS block - final cosig
    std::this_thread::sleep_for(
        std::chrono::milliseconds(DELAY_FIRSTXNEPOCH_IN_MS));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DSBlock to be sent to the lookup nodes");

    auto composeDSBlockMessageForSender = [this](bytes& message) -> bool {
      return ComposeDSBlockMessageForSender(message);
    };

    auto sendDSBlockToLookupNodesAndNewDSMembers =
        [this]([[gnu::unused]] const VectorOfLookupNode& lookups,
               const bytes& message) -> void {
      SendDSBlockToLookupNodesAndNewDSMembers(message);
    };

    auto sendDSBlockToShardNodes =
        [this](const bytes& message, const DequeOfShard& shards,
               const unsigned int& my_shards_lo,
               const unsigned int& my_shards_hi) -> void {
      SendDSBlockToShardNodes(message, shards, my_shards_lo, my_shards_hi);
    };

    DataSender::GetInstance().SendDataToOthers(
        *m_pendingDSBlock, *(m_mediator.m_DSCommittee), m_shards, {},
        m_mediator.m_lookup->GetLookupNodes(),
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(),
        m_consensusMyID, composeDSBlockMessageForSender,
        sendDSBlockToLookupNodesAndNewDSMembers, sendDSBlockToShardNodes);
  }

  LOG_STATE(
      "[DSBLK]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] AFTER SENDING DSBLOCK");

  ClearVCBlockVector();
  UpdateDSCommiteeComposition();
  UpdateMyDSModeAndConsensusId();

  if (m_mediator.m_DSCommittee->at(m_consensusLeaderID).first ==
      m_mediator.m_selfKey.second) {
    LOG_GENERAL(INFO, "New leader is at index " << m_consensusLeaderID << " "
                                                << m_mediator.m_selfPeer);
  } else {
    LOG_GENERAL(
        INFO, "New leader is at index "
                  << m_consensusLeaderID << " "
                  << m_mediator.m_DSCommittee->at(m_consensusLeaderID).second);
  }

  LOG_GENERAL(INFO, "DS committee");
  for (const auto& member : *m_mediator.m_DSCommittee) {
    LOG_GENERAL(INFO, member.second);
  }

  BlockStorage::GetBlockStorage().PutDSCommittee(m_mediator.m_DSCommittee,
                                                 m_consensusLeaderID);

  StartFirstTxEpoch();
}

bool DirectoryService::ProcessDSBlockConsensus(
    const bytes& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessDSBlockConsensus not expected to "
                "be called from LookUp node.");
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

    // Wait until ProcessDSBlock in the case that primary sent announcement
    // pretty early
    if ((m_state == POW_SUBMISSION) || (m_state == DSBLOCK_CONSENSUS_PREP) ||
        (m_state == VIEWCHANGE_CONSENSUS)) {
      cv_DSBlockConsensus.notify_all();

      std::unique_lock<std::mutex> cv_lk(m_MutexCVDSBlockConsensusObject);

      if (cv_DSBlockConsensusObject.wait_for(
              cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT)) ==
          std::cv_status::timeout) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Time out while waiting for state transition and "
                  "consensus object creation ");
      }

      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "State transition is completed and consensus object "
                "creation. (check for timeout)");
    }

    if (!CheckState(PROCESS_DSBLOCKCONSENSUS)) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Ignoring consensus message");
      return false;
    }
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
    LOG_GENERAL(WARNING,
                "Timeout while waiting for correct order of DS Block consensus "
                "messages");
    return false;
  }

  lock_guard<mutex> g(m_mutexConsensus);

  if (!m_consensusObject->ProcessMessage(message, offset, from)) {
    return false;
  }

  ConsensusCommon::State state = m_consensusObject->GetState();

  if (state == ConsensusCommon::State::DONE) {
    m_viewChangeCounter = 0;
    cv_viewChangeDSBlock.notify_all();
    ProcessDSBlockConsensusWhenDone(message, offset);
  } else if (state == ConsensusCommon::State::ERROR) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "No consensus reached. Wait for view change");
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG for verify sig m_allPoWConns  size is "
                  << m_allPoWConns.size()
                  << ". Please check numbers of pow receivied by this node");
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus state = " << m_consensusObject->GetStateString());
    cv_processConsensusMessage.notify_all();
  }

  return true;
}
