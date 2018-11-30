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
  vector<unsigned char> serializedDSBlock;
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

void DirectoryService::SendDSBlockToLookupNodes() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendDSBlockToLookupNodes not expected "
                "to be called from LookUp node.");
    return;
  }

  vector<unsigned char> dsblock_message = {MessageType::NODE,
                                           NodeInstructionType::DSBLOCK};
  if (!Messenger::SetNodeVCDSBlocksMessage(
          dsblock_message, MessageOffset::BODY, 0, *m_pendingDSBlock,
          m_VCBlockVector, m_shards, m_DSReceivers, m_shardReceivers,
          m_shardSenders)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetNodeVCDSBlocksMessage failed.");
    return;
  }

  m_mediator.m_lookup->SendMessageToLookupNodes(dsblock_message);
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I the part of the subset of the DS committee that have sent the "
            "DSBlock to the lookup nodes");
}

void DirectoryService::SendDSBlockToNewDSMembers() {
  LOG_MARKER();
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendDSBlockToNewDSMembers not expected "
                "to be called from LookUp node.");
    return;
  }

  vector<unsigned char> dsblock_message = {MessageType::NODE,
                                           NodeInstructionType::DSBLOCK};
  if (!Messenger::SetNodeVCDSBlocksMessage(
          dsblock_message, MessageOffset::BODY, 0, *m_pendingDSBlock,
          m_VCBlockVector, m_shards, m_DSReceivers, m_shardReceivers,
          m_shardSenders)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetNodeVCDSBlocksMessage failed.");
    return;
  }

  vector<Peer> newDSmembers;
  for (const auto& newDSmember :
       m_pendingDSBlock->GetHeader().GetDSPoWWinners()) {
    newDSmembers.emplace_back(newDSmember.second);
  }

  P2PComm::GetInstance().SendMessage(newDSmembers, dsblock_message);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I the part of the subset of the DS committee that have sent the "
            "DSBlock to the new DS leader");
}

void DirectoryService::SetupMulticastConfigForDSBlock(
    unsigned int& my_DS_cluster_num, unsigned int& my_shards_lo,
    unsigned int& my_shards_hi) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SetupMulticastConfigForDSBlock not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  unsigned int num_DS_clusters =
      m_mediator.m_DSCommittee->size() / DS_MULTICAST_CLUSTER_SIZE;
  if ((m_mediator.m_DSCommittee->size() % DS_MULTICAST_CLUSTER_SIZE) > 0) {
    // If there are still ds lefts, add a new ds cluster
    num_DS_clusters++;
  }

  unsigned int shard_groups_count = m_shards.size() / num_DS_clusters;
  if ((m_shards.size() % num_DS_clusters) > 0) {
    // If there is still nodes, increase num of shard
    shard_groups_count++;
  }

  my_DS_cluster_num = m_consensusMyID / DS_MULTICAST_CLUSTER_SIZE;
  my_shards_lo = my_DS_cluster_num * shard_groups_count;
  my_shards_hi = my_shards_lo + shard_groups_count -
                 1;  // Multicast configuration to my assigned shard's nodes -
                     // send SHARDING message
  if (my_shards_hi >= m_shards.size()) {
    my_shards_hi = m_shards.size() - 1;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "my_shards_lo: " << my_shards_lo
                             << " my_shards_hi: " << my_shards_hi
                             << " my_DS_cluster_num  : " << my_DS_cluster_num);
  LOG_EPOCH(
      INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
      "shard_groups_count : " << shard_groups_count
                              << " m shard size       : " << m_shards.size());
}

void DirectoryService::SendDSBlockToShardNodes(
    const unsigned int my_shards_lo, const unsigned int my_shards_hi) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendDSBlockToShardNodes not expected to "
                "be called from LookUp node.");
    return;
  }
  auto p = m_shards.begin();
  advance(p, my_shards_lo);

  for (unsigned int i = my_shards_lo; i <= my_shards_hi; i++) {
    // Get the shard ID from the leader's info in m_publicKeyToshardIdMap
    uint32_t shardId =
        m_publicKeyToshardIdMap.at(std::get<SHARD_NODE_PUBKEY>(p->front()));

    // Generate the message
    vector<unsigned char> dsblock_message = {MessageType::NODE,
                                             NodeInstructionType::DSBLOCK};
    if (!Messenger::SetNodeVCDSBlocksMessage(
            dsblock_message, MessageOffset::BODY, shardId, *m_pendingDSBlock,
            m_VCBlockVector, m_shards, m_DSReceivers, m_shardReceivers,
            m_shardSenders)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetNodeVCDSBlocksMessage failed.");
      return;
    }

    // Send the message
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(dsblock_message);
    vector<unsigned char> this_msg_hash = sha256.Finalize();

    LOG_STATE(
        "[INFOR]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6) << "]["
        << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
               .substr(0, 6)
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] SHMSG");

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
                                                  dsblock_message);
    } else {
      vector<Peer> shard_peers;
      for (const auto& kv : *p) {
        shard_peers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
      }
      P2PComm::GetInstance().SendBroadcastMessage(shard_peers, dsblock_message);
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
                "I am now Leader DS");
      LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                    DS_LEADER_MSG);
      m_mode = PRIMARY_DS;
    } else {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "I am now backup DS");
      LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                    DS_BACKUP_MSG);
      m_mode = BACKUP_DS;
    }

    LOG_STATE("[IDENT][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << setw(6) << left << m_consensusMyID
                         << "] DSBK");
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
    m_mediator.m_node->m_consensusLeaderID = m_consensusLeaderID;

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
        "[MIBLKSWAIT["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] BEGIN");

    m_stopRecvNewMBSubmission = false;

    if (BROADCAST_GOSSIP_MODE) {
      std::vector<Peer> peers;
      for (const auto& i : *m_mediator.m_node->m_myShardMembers) {
        if (i.second != Peer()) {
          peers.emplace_back(i.second);
        }
      }
      // ReInitialize RumorManager for this epoch.
      P2PComm::GetInstance().InitializeRumorManager(peers);
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

        LOG_STATE("[MIBLKSWAIT["
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

    // Process txn sharing assignments as a shard node
    m_mediator.m_node->LoadTxnSharingInfo();

    // Finally, start as a shard node
    m_mediator.m_node->StartFirstTxEpoch();
  }
}

void DirectoryService::ProcessDSBlockConsensusWhenDone(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset) {
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
    m_DSReceivers = std::move(m_tempDSReceivers);
    m_shardReceivers = std::move(m_tempShardReceivers);
    m_shardSenders = std::move(m_tempShardSenders);
    m_shards = std::move(m_tempShards);
    m_publicKeyToshardIdMap = std::move(m_tempPublicKeyToshardIdMap);
    m_mapNodeReputation = std::move(m_tempMapNodeReputation);
    ProcessTxnBodySharingAssignment();
  }

  BlockStorage::GetBlockStorage().PutShardStructure(m_shards);

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
    // TODO: Refine this
    unsigned int nodeToSendToLookUpLo = m_mediator.GetShardSize(true) / 4;
    unsigned int nodeToSendToLookUpHi =
        nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

    if (m_consensusMyID > nodeToSendToLookUpLo &&
        m_consensusMyID < nodeToSendToLookUpHi) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "I the DS folks that will soon be sending the DSBlock to the "
                "lookup nodes");
      SendDSBlockToLookupNodes();
    }

    // Let's reuse the same DS nodes to send the DS Block to the new DS leader
    // Why is this done separately?  Because the new DS leader is not part of
    // m_shards. In multicast code below, we use m_shards as the basis for
    // sending to all the shard nodes.

    if (m_consensusMyID > nodeToSendToLookUpLo &&
        m_consensusMyID < nodeToSendToLookUpHi) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Part of the DS Committee that will sending the DSBlock to the "
                "new DS members");
      SendDSBlockToNewDSMembers();
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New DSBlock hash is  = 0x"
                  << DataConversion::charArrToHexStr(m_mediator.m_dsBlockRand)
                  << "\n");

    unsigned int my_DS_cluster_num, my_shards_lo, my_shards_hi;
    SetupMulticastConfigForDSBlock(my_DS_cluster_num, my_shards_lo,
                                   my_shards_hi);

    LOG_STATE(
        "[DSBLK]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] BEFORE SENDING DSBLOCK");

    // Too few target nodes - avoid asking all DS clusters to send
    if ((my_DS_cluster_num + 1) <= m_shards.size()) {
      SendDSBlockToShardNodes(my_shards_lo, my_shards_hi);
    }
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
    const vector<unsigned char>& message, unsigned int offset,
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
