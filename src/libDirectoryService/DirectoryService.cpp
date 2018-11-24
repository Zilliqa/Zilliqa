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
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/RootComputation.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

DirectoryService::DirectoryService(Mediator& mediator) : m_mediator(mediator) {
  if (!LOOKUP_NODE_MODE) {
    SetState(POW_SUBMISSION);
    cv_POWSubmission.notify_all();
  }
  m_mode = IDLE;
  m_consensusLeaderID = 0;
  m_mediator.m_consensusID = 1;
  m_viewChangeCounter = 0;
}

DirectoryService::~DirectoryService() {}

void DirectoryService::StartSynchronization() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::StartSynchronization not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  this->CleanVariables();

  if (!m_mediator.m_node->GetOfflineLookups()) {
    LOG_GENERAL(WARNING, "Cannot sync currently");
    return;
  }

  auto func = [this]() -> void {
    while (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC) {
      m_mediator.m_lookup->ComposeAndSendGetDirectoryBlocksFromSeed(
          m_mediator.m_blocklinkchain.GetLatestIndex() + 1);
      m_synchronizer.FetchLatestTxBlocks(
          m_mediator.m_lookup,
          m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
              1);
      this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
    }
  };

  auto func2 = [this]() -> void {
    if (!m_mediator.m_lookup->GetDSInfoLoop()) {
      LOG_GENERAL(WARNING, "Unable to fetch DS info");
    }
  };
  DetachedFunction(1, func);
  DetachedFunction(1, func2);
}

bool DirectoryService::CheckState(Action action) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckState not "
                "expected to be called from LookUp node.");
    return true;
  }

  if (m_mode == Mode::IDLE) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a non-DS node now. Why am I getting this message?");
    return false;
  }

  static const std::multimap<DirState, Action> ACTIONS_FOR_STATE = {
      {POW_SUBMISSION, PROCESS_POWSUBMISSION},
      {POW_SUBMISSION, VERIFYPOW},
      {DSBLOCK_CONSENSUS, PROCESS_DSBLOCKCONSENSUS},
      {MICROBLOCK_SUBMISSION, PROCESS_MICROBLOCKSUBMISSION},
      {FINALBLOCK_CONSENSUS, PROCESS_FINALBLOCKCONSENSUS},
      {VIEWCHANGE_CONSENSUS, PROCESS_VIEWCHANGECONSENSUS}};

  bool found = false;

  for (auto pos = ACTIONS_FOR_STATE.lower_bound(m_state);
       pos != ACTIONS_FOR_STATE.upper_bound(m_state); pos++) {
    if (pos->second == action) {
      found = true;
      break;
    }
  }

  if (!found) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Action " << GetActionString(action) << " not allowed in state "
                        << GetStateString());
    return false;
  }

  return true;
}

bool DirectoryService::ProcessSetPrimary(const vector<unsigned char>& message,
                                         unsigned int offset,
                                         [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessSetPrimary not "
                "expected to be called from LookUp node.");
    return true;
  }

  // Note: This function should only be invoked during bootstrap sequence
  // Message = [Primary node IP] [Primary node port]
  LOG_MARKER();

  if (m_mediator.m_currentEpochNum > 1) {
    // TODO: Get the IP address of who send this message, and deduct its
    // reputation.
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessSetPrimary is a bootstrap "
                "function, it shouldn't be called after blockchain "
                "started.");
    return false;
  }

  // Peer primary(message, offset);
  Peer primary;
  if (primary.Deserialize(message, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to deserialize Peer.");
    return false;
  }

  if (primary == m_mediator.m_selfPeer) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the DS committee leader");
    LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                  DS_LEADER_MSG);
    m_mode = PRIMARY_DS;
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a DS committee backup. "
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << ":"
                  << m_mediator.m_selfPeer.m_listenPortHost);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Current DS committee leader is "
                  << primary.GetPrintableIPAddress() << " at port "
                  << primary.m_listenPortHost)
    LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                  DS_BACKUP_MSG);
    m_mode = BACKUP_DS;
  }

  // For now, we assume the following when ProcessSetPrimary() is called:
  //  1. All peers in the peer list are my fellow DS committee members for this
  //  first epoch
  //  2. The list of DS nodes is sorted by PubKey, including my own
  //  3. The peer with the smallest PubKey is also the first leader assigned in
  //  this call to ProcessSetPrimary()

  // Let's notify lookup node of the DS committee during bootstrap
  // TODO: Refactor this code
  if (primary == m_mediator.m_selfPeer) {
    PeerStore& dsstore = PeerStore::GetStore();
    dsstore.AddPeerPair(
        m_mediator.m_selfKey.second,
        m_mediator.m_selfPeer);  // Add myself, but with dummy IP info
    vector<pair<PubKey, Peer>> ds = dsstore.GetAllPeerPairs();
    m_mediator.m_DSCommittee->resize(ds.size());
    copy(ds.begin(), ds.end(), m_mediator.m_DSCommittee->begin());

    vector<unsigned char> setDSBootstrapNodeMessage = {
        MessageType::LOOKUP, LookupInstructionType::SETDSINFOFROMSEED};

    if (!Messenger::SetLookupSetDSInfoFromSeed(
            setDSBootstrapNodeMessage, MessageOffset::BODY,
            m_mediator.m_selfKey, *m_mediator.m_DSCommittee, false)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetLookupSetDSInfoFromSeed failed.");
      return false;
    }

    m_mediator.m_lookup->SendMessageToLookupNodes(setDSBootstrapNodeMessage);
  }

  PeerStore& peerstore = PeerStore::GetStore();
  peerstore.AddPeerPair(m_mediator.m_selfKey.second,
                        Peer());  // Add myself, but with dummy IP info

  vector<pair<PubKey, Peer>> tmp1 = peerstore.GetAllPeerPairs();
  m_mediator.m_DSCommittee->resize(tmp1.size());
  copy(tmp1.begin(), tmp1.end(), m_mediator.m_DSCommittee->begin());
  peerstore.RemovePeer(m_mediator.m_selfKey.second);  // Remove myself

  // Lets start the gossip as earliest as possible
  if (BROADCAST_GOSSIP_MODE) {
    std::vector<Peer> peers;
    for (const auto& i : *m_mediator.m_DSCommittee) {
      if (i.second.m_listenPortHost != 0) {
        peers.emplace_back(i.second);
      }
    }
    P2PComm::GetInstance().InitializeRumorManager(peers);
  }

  // Now I need to find my index in the sorted list (this will be my ID for the
  // consensus)
  m_consensusMyID = 0;

  {
    lock_guard<mutex> g(m_mediator.m_mutexInitialDSCommittee);
    if (m_mediator.m_DSCommittee->size() !=
        m_mediator.m_initialDSCommittee->size()) {
      LOG_GENERAL(WARNING,
                  "The initial DS committee from file and ProcessSetPrimary "
                  "size do not match "
                      << m_mediator.m_DSCommittee->size() << " "
                      << m_mediator.m_initialDSCommittee->size());
    }
    for (unsigned int i = 0; i < m_mediator.m_initialDSCommittee->size(); i++) {
      if (!(m_mediator.m_DSCommittee->at(i).first ==
            m_mediator.m_initialDSCommittee->at(i))) {
        LOG_GENERAL(WARNING,
                    "PubKey from file and ProcessSetPrimary do not match  "
                        << m_mediator.m_DSCommittee->at(i).first << " "
                        << m_mediator.m_initialDSCommittee->at(i))
      }
    }
  }

  for (auto const& i : *m_mediator.m_DSCommittee) {
    if (i.first == m_mediator.m_selfKey.second) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "My node ID for this PoW consensus is " << m_consensusMyID);
      break;
    }
    m_consensusMyID++;
  }

  m_consensusLeaderID = 0;
  if (m_mediator.m_currentEpochNum > 1) {
    LOG_GENERAL(WARNING, "ProcessSetPrimary called in epoch "
                             << m_mediator.m_currentEpochNum);
    m_consensusLeaderID =
        DataConversion::charArrTo16Bits(
            m_mediator.m_dsBlockChain.GetLastBlock().GetBlockHash().asBytes()) %
        m_mediator.m_DSCommittee->size();
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "START OF EPOCH " << m_mediator.m_dsBlockChain.GetLastBlock()
                                         .GetHeader()
                                         .GetBlockNum() +
                                     1);

  if (primary == m_mediator.m_selfPeer) {
    LOG_STATE("[IDENT][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][0     ] DSLD");
  } else {
    LOG_STATE("[IDENT][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << std::setw(6) << std::left << m_consensusMyID
                         << "] DSBK");
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Waiting " << POW_WINDOW_IN_SECONDS
                       << " seconds, accepting PoW submissions...");
  this_thread::sleep_for(chrono::seconds(POW_WINDOW_IN_SECONDS));
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Starting consensus on ds block");
  RunConsensusOnDSBlock();

  return true;
}

bool DirectoryService::CheckWhetherDSBlockIsFresh(const uint64_t dsblock_num) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckWhetherDSBlockIsFresh not expected "
                "to be called from LookUp node.");
    return true;
  }

  // uint128_t latest_block_num_in_blockchain =
  // m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t latest_block_num_in_blockchain =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (dsblock_num < latest_block_num_in_blockchain + 1) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "We are processing duplicated blocks");
    return false;
  } else if (dsblock_num > latest_block_num_in_blockchain + 1) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Warning: We are missing of some DS blocks. Cur: "
                  << dsblock_num
                  << ". New: " << latest_block_num_in_blockchain);
    // Todo: handle missing DS blocks.
    return false;
  }
  return true;
}

void DirectoryService::SetState(DirState state) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SetState not expected to be called from "
                "LookUp node.");
    return;
  }

  m_state = state;
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DS State is now " << GetStateString());
}

vector<Peer> DirectoryService::GetBroadcastList(
    [[gnu::unused]] unsigned char ins_type,
    [[gnu::unused]] const Peer& broadcast_originator) {
  // LOG_MARKER();
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::GetBroadcastList not expected to be "
                "called from LookUp node.");
  }

  // Regardless of the instruction type, right now all our "broadcasts" are just
  // redundant multicasts from DS nodes to non-DS nodes
  return vector<Peer>();
}

bool DirectoryService::CleanVariables() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CleanVariables not expected to be "
                "called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  m_shards.clear();
  m_publicKeyToshardIdMap.clear();
  m_allPoWConns.clear();
  m_mapNodeReputation.clear();

  m_stopRecvNewMBSubmission = false;
  m_needCheckMicroBlock = true;
  m_startedRunFinalblockConsensus = false;

  {
    std::lock_guard<mutex> lock(m_mutexConsensus);
    m_consensusObject.reset();
  }

  m_consensusBlockHash.clear();
  {
    std::lock_guard<mutex> lock(m_mutexPendingDSBlock);
    m_pendingDSBlock.reset();
  }
  {
    std::lock_guard<mutex> lock(m_mutexAllPOW);
    m_allPoWs.clear();
  }

  ClearDSPoWSolns();

  ResetPoWSubmissionCounter();

  {
    std::lock_guard<mutex> lock(m_mutexMicroBlocks);
    m_microBlocks.clear();
    m_microBlockStateDeltas.clear();
    m_missingMicroBlocks.clear();
    m_totalTxnFees = 0;
  }
  CleanFinalblockConsensusBuffer();

  m_finalBlock.reset();
  m_sharingAssignment.clear();
  m_viewChangeCounter = 0;
  m_mode = IDLE;
  m_consensusLeaderID = 0;
  m_mediator.m_consensusID = 0;

  return true;
}

void DirectoryService::RejoinAsDS() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RejoinAsDS not expected to be called "
                "from LookUp node.");
    return;
  }

  LOG_MARKER();
  if (m_mediator.m_lookup->m_syncType == SyncType::NO_SYNC &&
      m_mode == BACKUP_DS) {
    auto func = [this]() mutable -> void {
      m_mediator.m_lookup->m_syncType = SyncType::DS_SYNC;
      m_mediator.m_node->CleanVariables();
      m_mediator.m_node->Install(SyncType::DS_SYNC, true);
      this->StartSynchronization();
    };
    DetachedFunction(1, func);
  }
}

bool DirectoryService::FinishRejoinAsDS() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::FinishRejoinAsDS not expected to be "
                "called from LookUp node.");
    return true;
  }

  LOG_MARKER();
  m_mode = BACKUP_DS;
  unsigned int dsSize = 0;
  {
    std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    dsSize = m_mediator.m_DSCommittee->size();
    LOG_GENERAL(INFO,
                "m_DSCommittee size: " << m_mediator.m_DSCommittee->size());
    if (dsSize == 0) {
      LOG_GENERAL(WARNING, "DS committee unset, failed to rejoin");
      return false;
    }
  }

  m_consensusLeaderID = 0;
  BlockLink bl = m_mediator.m_blocklinkchain.GetLatestBlockLink();
  const auto& blocktype = get<BlockLinkIndex::BLOCKTYPE>(bl);
  PubKey leaderPubKey;
  if (blocktype == BlockType::DS) {
    if (!GUARD_MODE){
      m_consensusLeaderID =
          DataConversion::charArrTo16Bits(
              m_mediator.m_dsBlockChain.GetLastBlock().GetBlockHash().asBytes()) %
          dsSize;
    }else {
      m_consensusLeaderID =
          DataConversion::charArrTo16Bits(
              m_mediator.m_dsBlockChain.GetLastBlock().GetBlockHash().asBytes()) %
          Guard::GetInstance().GetNumOfDSGuard();
    }

  } else if (blocktype == BlockType::VC) {
    VCBlockSharedPtr VCBlockptr;
    if (!BlockStorage::GetBlockStorage().GetVCBlock(
            get<BlockLinkIndex::BLOCKHASH>(bl), VCBlockptr)) {
      LOG_GENERAL(FATAL, "could not get vc block "
                             << get<BlockLinkIndex::BLOCKHASH>(bl));
      for (auto const& i : *m_mediator.m_DSCommittee) {
        if (i.first == leaderPubKey) {
          LOG_EPOCH(
              INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Leader ID for this PoW consensus is " << m_consensusLeaderID);
          break;
        }
        m_consensusLeaderID++;
      }
    }
    leaderPubKey = VCBlockptr->GetHeader().GetCandidateLeaderPubKey();
  } else {
    m_mediator.m_node->RejoinAsNormal();
  }

  m_consensusMyID = 0;
  bool found = false;
  {
    std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    for (auto const& i : *m_mediator.m_DSCommittee) {
      if (i.first == m_mediator.m_selfKey.second) {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "My node ID for this PoW consensus is " << m_consensusMyID);
        found = true;
        break;
      }
      m_consensusMyID++;
    }
  }
  if (!found) {
    LOG_GENERAL(
        WARNING,
        "Unable to find myself in ds committee, Invoke Rejoin as Normal");
    m_mediator.m_node->RejoinAsNormal();
    return false;
  }

  // in case the recovery program is under different directory
  LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(), DS_BACKUP_MSG);
  RunConsensusOnDSBlock(true);
  return true;
}

void DirectoryService::DetermineShardsToSendBlockTo(
    unsigned int& my_DS_cluster_num, unsigned int& my_shards_lo,
    unsigned int& my_shards_hi) {
  // Multicast block to my assigned shard's nodes - send BLOCK message
  // Message = [block]

  // Multicast assignments:
  // 1. Divide DS committee into clusters of size 20
  // 2. Each cluster talks to all shard members in each shard
  //    DS cluster 0 => Shard 0
  //    DS cluster 1 => Shard 1
  //    ...
  //    DS cluster 0 => Shard (num of DS clusters)
  //    DS cluster 1 => Shard (num of DS clusters + 1)
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::DetermineShardsToSendBlockTo not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  unsigned int num_DS_clusters =
      m_mediator.m_DSCommittee->size() / DS_MULTICAST_CLUSTER_SIZE;
  if ((m_mediator.m_DSCommittee->size() % DS_MULTICAST_CLUSTER_SIZE) > 0) {
    num_DS_clusters++;
  }
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DEBUG num of ds clusters " << num_DS_clusters)
  unsigned int shard_groups_count = m_shards.size() / num_DS_clusters;
  if ((m_shards.size() % num_DS_clusters) > 0) {
    shard_groups_count++;
  }
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DEBUG num of shard group count " << shard_groups_count)

  my_DS_cluster_num = m_consensusMyID / DS_MULTICAST_CLUSTER_SIZE;
  my_shards_lo = my_DS_cluster_num * shard_groups_count;
  my_shards_hi = my_shards_lo + shard_groups_count - 1;

  if (my_shards_hi >= m_shards.size()) {
    my_shards_hi = m_shards.size() - 1;
  }
}

void DirectoryService::StartNewDSEpochConsensus(bool fromFallback) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::StartNewDSEpochConsensus not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  m_mediator.m_consensusID = 0;
  m_mediator.m_node->m_consensusLeaderID = 0;

  CleanFinalblockConsensusBuffer();

  m_mediator.m_node->CleanCreatedTransaction();

  m_mediator.m_node->CleanMicroblockConsensusBuffer();

  SetState(POW_SUBMISSION);
  cv_POWSubmission.notify_all();

  POW::GetInstance().EthashConfigureClient(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1,
      FULL_DATASET_MINE);

  if (m_mode == PRIMARY_DS) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                                (fromFallback ? FALLBACK_EXTRA_TIME : 0)
                         << " seconds, accepting PoW submissions...");

    // Notify lookup that it's time to do PoW
    vector<unsigned char> startpow_message = {
        MessageType::LOOKUP, LookupInstructionType::RAISESTARTPOW};
    m_mediator.m_lookup->SendMessageToLookupNodesSerial(startpow_message);

    // New nodes poll DSInfo from the lookups every NEW_NODE_SYNC_INTERVAL
    // So let's add that to our wait time to allow new nodes to get SETSTARTPOW
    // and submit a PoW
    this_thread::sleep_for(
        chrono::seconds(NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                        (fromFallback ? FALLBACK_EXTRA_TIME : 0)));

    RunConsensusOnDSBlock();
  } else {
    std::unique_lock<std::mutex> cv_lk(m_MutexCVDSBlockConsensus);

    // New nodes poll DSInfo from the lookups every NEW_NODE_SYNC_INTERVAL
    // So let's add that to our wait time to allow new nodes to get SETSTARTPOW
    // and submit a PoW
    if (cv_DSBlockConsensus.wait_for(
            cv_lk, std::chrono::seconds(
                       NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                       (fromFallback ? FALLBACK_EXTRA_TIME : 0))) ==
        std::cv_status::timeout) {
      LOG_GENERAL(INFO, "Woken up from the sleep of "
                            << NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                                   (fromFallback ? FALLBACK_EXTRA_TIME : 0)
                            << " seconds");
    } else {
      LOG_GENERAL(INFO,
                  "Received announcement message. Time to "
                  "run consensus.");
    }

    RunConsensusOnDSBlock();
  }
}

void DirectoryService::SendBlockToShardNodes(
    unsigned int my_DS_cluster_num, unsigned int my_shards_lo,
    unsigned int my_shards_hi, vector<unsigned char>& block_message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SendBlockToShardNodes not expected to "
                "be called from LookUp node.");
    return;
  }
  // Too few target shards - avoid asking all DS clusters to send
  LOG_MARKER();

  if ((my_DS_cluster_num + 1) <= m_shards.size()) {
    auto p = m_shards.begin();
    advance(p, my_shards_lo);

    for (unsigned int i = my_shards_lo; i <= my_shards_hi; i++) {
      if (BROADCAST_TREEBASED_CLUSTER_MODE) {
        // Choose N other Shard nodes to be recipient of block
        std::vector<Peer> shardBlockReceivers;

        SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
        sha256.Update(block_message);
        vector<unsigned char> this_msg_hash = sha256.Finalize();

        LOG_GENERAL(
            INFO,
            "Sending message with hash: ["
                << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
                << "] to NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD:"
                << NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD << " shard peers");

        unsigned int numOfBlockReceivers = std::min(
            NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD, (uint32_t)p->size());

        for (unsigned int i = 0; i < numOfBlockReceivers; i++) {
          const auto& kv = p->at(i);
          shardBlockReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));

          LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                    " PubKey: "
                        << DataConversion::SerializableToHexStr(
                               std::get<SHARD_NODE_PUBKEY>(kv))
                        << " IP: "
                        << std::get<SHARD_NODE_PEER>(kv).GetPrintableIPAddress()
                        << " Port: "
                        << std::get<SHARD_NODE_PEER>(kv).m_listenPortHost);
        }

        P2PComm::GetInstance().SendBroadcastMessage(shardBlockReceivers,
                                                    block_message);
      } else {
        vector<Peer> shard_peers;

        for (const auto& kv : *p) {
          shard_peers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
          LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                    " PubKey: "
                        << DataConversion::SerializableToHexStr(
                               std::get<SHARD_NODE_PUBKEY>(kv))
                        << " IP: "
                        << std::get<SHARD_NODE_PEER>(kv).GetPrintableIPAddress()
                        << " Port: "
                        << std::get<SHARD_NODE_PEER>(kv).m_listenPortHost);
        }

        P2PComm::GetInstance().SendBroadcastMessage(shard_peers, block_message);
      }

      p++;
    }
  }
}

bool DirectoryService::ToBlockMessage([[gnu::unused]] unsigned char ins_byte) {
  return m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC;
}

bool DirectoryService::Execute(const vector<unsigned char>& message,
                               unsigned int offset, const Peer& from) {
  // LOG_MARKER();

  bool result = false;

  typedef bool (DirectoryService::*InstructionHandler)(
      const vector<unsigned char>&, unsigned int, const Peer&);

  std::vector<InstructionHandler> ins_handlers;

  if (!LOOKUP_NODE_MODE) {
    ins_handlers.insert(ins_handlers.end(),
                        {&DirectoryService::ProcessSetPrimary,
                         &DirectoryService::ProcessPoWSubmission,
                         &DirectoryService::ProcessDSBlockConsensus,
                         &DirectoryService::ProcessMicroblockSubmission,
                         &DirectoryService::ProcessFinalBlockConsensus,
                         &DirectoryService::ProcessViewChangeConsensus,
                         &DirectoryService::ProcessGetDSTxBlockMessage});
  } else {
    ins_handlers.insert(ins_handlers.end(),
                        {&DirectoryService::ProcessSetPrimary,
                         &DirectoryService::ProcessPoWSubmission,
                         &DirectoryService::ProcessDSBlockConsensus,
                         &DirectoryService::ProcessMicroblockSubmission,
                         &DirectoryService::ProcessFinalBlockConsensus});
  }

  const unsigned char ins_byte = message.at(offset);

  const unsigned int ins_handlers_count = ins_handlers.size();

  if (ToBlockMessage(ins_byte)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Ignore DS message");
    return false;
  }

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);

    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Unknown instruction byte " << hex << (unsigned int)ins_byte);
  }

  return result;
}

#define MAKE_LITERAL_PAIR(s) \
  { s, #s }

map<DirectoryService::DirState, string> DirectoryService::DirStateStrings = {
    MAKE_LITERAL_PAIR(POW_SUBMISSION),
    MAKE_LITERAL_PAIR(DSBLOCK_CONSENSUS_PREP),
    MAKE_LITERAL_PAIR(DSBLOCK_CONSENSUS),
    MAKE_LITERAL_PAIR(MICROBLOCK_SUBMISSION),
    MAKE_LITERAL_PAIR(FINALBLOCK_CONSENSUS_PREP),
    MAKE_LITERAL_PAIR(FINALBLOCK_CONSENSUS),
    MAKE_LITERAL_PAIR(VIEWCHANGE_CONSENSUS_PREP),
    MAKE_LITERAL_PAIR(VIEWCHANGE_CONSENSUS),
    MAKE_LITERAL_PAIR(ERROR)};

string DirectoryService::GetStateString() const {
  return (DirStateStrings.find(m_state) == DirStateStrings.end())
             ? "Unknown"
             : DirStateStrings.at(m_state);
}

map<DirectoryService::Action, string> DirectoryService::ActionStrings = {
    MAKE_LITERAL_PAIR(PROCESS_POWSUBMISSION),
    MAKE_LITERAL_PAIR(VERIFYPOW),
    MAKE_LITERAL_PAIR(PROCESS_DSBLOCKCONSENSUS),
    MAKE_LITERAL_PAIR(PROCESS_MICROBLOCKSUBMISSION),
    MAKE_LITERAL_PAIR(PROCESS_FINALBLOCKCONSENSUS),
    MAKE_LITERAL_PAIR(PROCESS_VIEWCHANGECONSENSUS)};

std::string DirectoryService::GetActionString(Action action) const {
  return (ActionStrings.find(action) == ActionStrings.end())
             ? "Unknown"
             : ActionStrings.at(action);
}

uint8_t DirectoryService::CalculateNewDifficulty(
    const uint8_t& currentDifficulty) {
  constexpr unsigned int MAX_ADJUST_THRESHOLD = 99;

  int64_t currentNodes = 0, powSubmissions = 0;
  {
    lock_guard<mutex> g(m_mutexAllPOW);
    powSubmissions = m_allPoWs.size();

    for (const auto& shard : m_shards) {
      currentNodes += shard.size();
    }
  }

  LOG_EPOCH(INFO, std::to_string(m_mediator.m_currentEpochNum).c_str(),
            "currentDifficulty " << std::to_string(currentDifficulty)
                                 << ", currentNodes " << currentNodes
                                 << ", powSubmissions " << powSubmissions);
  return CalculateNewDifficultyCore(
      currentDifficulty, POW_DIFFICULTY, currentNodes, powSubmissions,
      NUM_NODE_INCR_DIFFICULTY, MAX_ADJUST_THRESHOLD,
      m_mediator.m_currentEpochNum, CalculateNumberOfBlocksPerYear());
}

uint8_t DirectoryService::CalculateNewDSDifficulty(
    const uint8_t& dsDifficulty) {
  constexpr unsigned int MAX_ADJUST_THRESHOLD = 9;

  int64_t currentDSNodes = m_mediator.m_DSCommittee->size();
  int64_t dsPowSubmissions = GetNumberOfDSPoWSolns();

  LOG_EPOCH(INFO, std::to_string(m_mediator.m_currentEpochNum).c_str(),
            "dsDifficulty " << std::to_string(dsDifficulty)
                            << ", currentDSNodes " << currentDSNodes
                            << ", dsPowSubmissions " << dsPowSubmissions);

  return CalculateNewDifficultyCore(
      dsDifficulty, DS_POW_DIFFICULTY, currentDSNodes, dsPowSubmissions,
      currentDSNodes, MAX_ADJUST_THRESHOLD, m_mediator.m_currentEpochNum,
      CalculateNumberOfBlocksPerYear());
}

uint8_t DirectoryService::CalculateNewDifficultyCore(
    uint8_t currentDifficulty, uint8_t minDifficulty, int64_t currentNodes,
    int64_t powSubmissions, int64_t expectedNodes, uint32_t maxAdjustThreshold,
    int64_t currentEpochNum, int64_t numBlockPerYear) {
  constexpr int8_t MAX_ADJUST_STEP = 2;
  constexpr float ONE_HUNDRED_PERCENT = 100.f;
  constexpr uint8_t MAX_INCREASE_DIFFICULTY_YEARS = 10;

  int64_t adjustment = 0;
  if (currentNodes > 0 && currentNodes != powSubmissions) {
    int64_t submissionsDiff;
    if (!SafeMath<int64_t>::sub(powSubmissions, currentNodes,
                                submissionsDiff)) {
      LOG_GENERAL(WARNING, "Calculate PoW submission difference goes wrong");
      submissionsDiff = 0;
    }

    // To make the adjustment work on small network.
    int64_t adjustThreshold = std::ceil(
        currentNodes * POW_CHANGE_PERCENT_TO_ADJ_DIFF / ONE_HUNDRED_PERCENT);
    if (adjustThreshold > maxAdjustThreshold) {
      adjustThreshold = maxAdjustThreshold;
    }

    // If the PoW submissions change not so big, then adjust according to the
    // expected whole network node number.
    if (abs(submissionsDiff) < adjustThreshold) {
      // If the PoW submissions exceeded the expected whole network node number,
      // then increase the difficulty.
      if (submissionsDiff > 0 && currentNodes > expectedNodes) {
        adjustment = 1;
      } else if (submissionsDiff < 0 && currentNodes < expectedNodes) {
        adjustment = -1;
      }
    } else {
      if (!SafeMath<int64_t>::div(submissionsDiff, adjustThreshold,
                                  adjustment)) {
        LOG_GENERAL(WARNING, "Calculate difficulty adjustment goes wrong");
        adjustment = 0;
      }
    }
  }

  // Restrict the adjustment step, prevent the difficulty jump up/down
  // dramatically.
  if (adjustment > MAX_ADJUST_STEP) {
    adjustment = MAX_ADJUST_STEP;
  } else if (adjustment < -MAX_ADJUST_STEP) {
    adjustment = -MAX_ADJUST_STEP;
  }

  uint8_t newDifficulty = std::max((uint8_t)(adjustment + currentDifficulty),
                                   (uint8_t)(minDifficulty));

  // Within 10 years, every year increase the difficulty by one.
  if (currentEpochNum / numBlockPerYear <= MAX_INCREASE_DIFFICULTY_YEARS &&
      currentEpochNum % numBlockPerYear == 0) {
    LOG_GENERAL(INFO, "At one year epoch " << currentEpochNum
                                           << ", increase difficulty by 1.");
    ++newDifficulty;
  }
  return newDifficulty;
}

uint64_t DirectoryService::CalculateNumberOfBlocksPerYear() const {
  // Every year, always increase the difficulty by 1, to encourage miners to
  // upgrade the hardware over time. If POW_WINDOW_IN_SECONDS = 300,
  // NUM_FINAL_BLOCK_PER_POW = 50, TX_DISTRIBUTE_TIME_IN_MS = 10000,
  // FINALBLOCK_DELAY_IN_MS = 3000, estimated blocks in a year is 1971000.
  uint64_t estimatedBlocksOneYear =
      365 * 24 * 3600 /
      ((POW_WINDOW_IN_SECONDS / NUM_FINAL_BLOCK_PER_POW) +
       ((TX_DISTRIBUTE_TIME_IN_MS + FINALBLOCK_DELAY_IN_MS) / 1000));

  // Round to integral multiple of NUM_FINAL_BLOCK_PER_POW
  estimatedBlocksOneYear = (estimatedBlocksOneYear / NUM_FINAL_BLOCK_PER_POW) *
                           NUM_FINAL_BLOCK_PER_POW;
  return estimatedBlocksOneYear;
}

int64_t DirectoryService::GetAllPoWSize() const {
  std::lock_guard<mutex> lock(m_mutexAllPOW);
  return m_allPoWs.size();
}