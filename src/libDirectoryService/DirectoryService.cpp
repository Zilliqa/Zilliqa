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
#include "libUtils/TimestampVerifier.h"

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
    while (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
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
        DataConversion::charArrTo16Bits(m_mediator.m_dsBlockChain.GetLastBlock()
                                            .GetHeader()
                                            .GetHashForRandom()
                                            .asBytes()) %
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

  if ((m_consensusMyID < POW_PACKET_SENDERS) ||
      (primary == m_mediator.m_selfPeer)) {
    LOG_GENERAL(INFO, "m_consensusMyID: " << m_consensusMyID);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << POW_WINDOW_IN_SECONDS
                         << " seconds, accepting PoW submissions...");
    this_thread::sleep_for(chrono::seconds(POW_WINDOW_IN_SECONDS));

    // create and send POW submission packets
    auto func = [this]() mutable -> void {
      this->ProcessAndSendPoWPacketSubmissionToOtherDSComm();
    };
    DetachedFunction(1, func);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << POWPACKETSUBMISSION_WINDOW_IN_SECONDS
                         << " seconds, accepting PoW submissions packet from "
                            "other DS member...");
    this_thread::sleep_for(
        chrono::seconds(POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
  } else {
    LOG_GENERAL(INFO, "m_consensusMyID: " << m_consensusMyID);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << POW_WINDOW_IN_SECONDS +
                                POWPACKETSUBMISSION_WINDOW_IN_SECONDS
                         << " seconds, accepting PoW submissions packets...");
    this_thread::sleep_for(chrono::seconds(
        POW_WINDOW_IN_SECONDS + POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
  }

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

  {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    m_mediator.m_DSCommittee->clear();
  }

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
  if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC &&
      m_mode == BACKUP_DS) {
    auto func = [this]() mutable -> void {
      m_mediator.m_lookup->SetSyncType(SyncType::DS_SYNC);
      m_mediator.m_node->CleanVariables();
      m_mediator.m_node->Install(SyncType::DS_SYNC);
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
  for (auto& i : *m_mediator.m_DSCommittee) {
    if (i.first == m_mediator.m_selfKey.second) {
      i.second = Peer();
      LOG_GENERAL(
          INFO,
          "Found current node to be inside ds committee. Setting it to Peer()");
      break;
    }
  }

  LOG_GENERAL(INFO, "DS committee is ");
  for (const auto& i : *m_mediator.m_DSCommittee) {
    LOG_GENERAL(INFO, i.second);
  }

  if (BROADCAST_GOSSIP_MODE) {
    std::vector<Peer> peers;
    for (const auto& i : *m_mediator.m_DSCommittee) {
      if (i.second.m_listenPortHost != 0) {
        peers.emplace_back(i.second);
      }
    }
    P2PComm::GetInstance().InitializeRumorManager(peers);
  }

  if (m_awaitingToSubmitNetworkInfoUpdate && GUARD_MODE) {
    UpdateDSGuardIdentity();
    LOG_GENERAL(
        INFO,
        "Sent ds guard network information update to lookup and ds committee")
  }

  m_mode = BACKUP_DS;
  DequeOfDSNode dsComm;
  {
    std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    LOG_GENERAL(INFO,
                "m_DSCommittee size: " << m_mediator.m_DSCommittee->size());
    if (m_mediator.m_DSCommittee->empty()) {
      LOG_GENERAL(WARNING, "DS committee unset, failed to rejoin");
      return false;
    }
    dsComm = *m_mediator.m_DSCommittee;
  }

  m_consensusLeaderID = 0;

  const auto& bl = m_mediator.m_blocklinkchain.GetLatestBlockLink();
  Peer dsLeaderPeer;
  if (Node::GetDSLeaderPeer(bl, m_mediator.m_dsBlockChain.GetLastBlock(),
                            dsComm, m_mediator.m_currentEpochNum,
                            dsLeaderPeer)) {
    auto iterDSLeader =
        std::find_if(dsComm.begin(), dsComm.end(),
                     [dsLeaderPeer](const std::pair<PubKey, Peer>& pubKeyPeer) {
                       return pubKeyPeer.second == dsLeaderPeer;
                     });
    if (iterDSLeader != dsComm.end()) {
      m_consensusLeaderID = iterDSLeader - dsComm.begin();
    } else {
      LOG_GENERAL(WARNING,
                  "Failed to find DS leader index in DS committee, Invoke "
                  "Rejoin as Normal");
      m_mediator.m_node->RejoinAsNormal();
      return false;
    }
  } else {
    LOG_GENERAL(WARNING,
                "Failed to get DS leader peer, Invoke Rejoin as Normal");
    m_mediator.m_node->RejoinAsNormal();
    return false;
  }

  m_consensusMyID = 0;
  bool found = false;

  for (auto const& i : dsComm) {
    if (i.first == m_mediator.m_selfKey.second) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "My node ID for this PoW consensus is " << m_consensusMyID);
      found = true;
      break;
    }
    m_consensusMyID++;
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
    // Notify lookup that it's time to do PoW
    vector<unsigned char> startpow_message = {
        MessageType::LOOKUP, LookupInstructionType::RAISESTARTPOW};
    m_mediator.m_lookup->SendMessageToLookupNodesSerial(startpow_message);

    // New nodes poll DSInfo from the lookups every NEW_NODE_SYNC_INTERVAL
    // So let's add that to our wait time to allow new nodes to get SETSTARTPOW
    // and submit a PoW

    LOG_GENERAL(INFO, "m_consensusMyID: " << m_consensusMyID);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                                (fromFallback ? FALLBACK_EXTRA_TIME : 0)
                         << " seconds, accepting PoW submissions...");

    this_thread::sleep_for(
        chrono::seconds(NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                        (fromFallback ? FALLBACK_EXTRA_TIME : 0)));

    // create and send POW submission packets
    auto func = [this]() mutable -> void {
      this->ProcessAndSendPoWPacketSubmissionToOtherDSComm();
    };
    DetachedFunction(1, func);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << POWPACKETSUBMISSION_WINDOW_IN_SECONDS
                         << " seconds, accepting PoW submissions packet from "
                            "other DS member...");

    this_thread::sleep_for(
        chrono::seconds(POWPACKETSUBMISSION_WINDOW_IN_SECONDS));

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
      // if i am suppose to create pow submission packet for other DS members
      if (m_consensusMyID < POW_PACKET_SENDERS) {
        // create and send POW submission packets
        LOG_GENERAL(INFO, "m_consensusMyID: " << m_consensusMyID);
        auto func = [this]() mutable -> void {
          this->ProcessAndSendPoWPacketSubmissionToOtherDSComm();
        };
        DetachedFunction(1, func);
      }

      if (cv_DSBlockConsensus.wait_for(
              cv_lk,
              std::chrono::seconds(POWPACKETSUBMISSION_WINDOW_IN_SECONDS)) ==
          std::cv_status::timeout) {
        LOG_GENERAL(INFO, "Woken up from the sleep of "
                              << POWPACKETSUBMISSION_WINDOW_IN_SECONDS
                              << " seconds");
      } else {
        LOG_GENERAL(INFO,
                    "Received announcement message. Time to "
                    "run consensus.");
      }
    } else {
      LOG_GENERAL(INFO,
                  "Received announcement message. Time to "
                  "run consensus.");
    }

    RunConsensusOnDSBlock();

    // now that we already run DSBlock Consensus, lets clear the buffered pow
    // solutions. why not clear it at start of new ds epoch - becoz sometimes
    // node is too late to start new ds epoch and and it already receives pow
    // solution for next ds epoch. so we buffer them instead.
    {
      lock_guard<mutex> g(m_mutexPowSolution);
      m_powSolutions.clear();
    }
  }
}

bool DirectoryService::ToBlockMessage([[gnu::unused]] unsigned char ins_byte) {
  return m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC;
}

// This feature is only available to ds guard nodes. This only guard nodes to
// change its network information. Pre-condition: Must still have access to
// existing public and private key pair
bool DirectoryService::UpdateDSGuardIdentity() {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING,
                "Not in guard mode. Unable to update ds guard network info.");
    return false;
  }

  if (!Guard::GetInstance().IsNodeInDSGuardList(m_mediator.m_selfKey.second)) {
    LOG_GENERAL(
        WARNING,
        "Current node is not a ds guard node. Unable to update network info.");
  }

  // To provide current pubkey, new IP, new Port and current timestamp
  vector<unsigned char> updatedsguardidentitymessage = {
      MessageType::DIRECTORY, DSInstructionType::NEWDSGUARDIDENTITY};

  uint64_t curDSEpochNo =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;

  if (!Messenger::SetDSLookupNewDSGuardNetworkInfo(
          updatedsguardidentitymessage, MessageOffset::BODY, curDSEpochNo,
          m_mediator.m_selfPeer, get_time_as_int(), m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetDSLookupNewDSGuardNetworkInfo failed.");
    return false;
  }

  // Send to all lookups
  m_mediator.m_lookup->SendMessageToLookupNodesSerial(
      updatedsguardidentitymessage);

  {
    // Gossip to all DS committee
    lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    vector<Peer> peerInfo;

    for (auto const& i : *m_mediator.m_DSCommittee) {
      if (i.second.m_listenPortHost != 0) {
        peerInfo.push_back(i.second);
      }
    }

    if (BROADCAST_GOSSIP_MODE) {
      P2PComm::GetInstance().SpreadRumor(updatedsguardidentitymessage);
    } else {
      P2PComm::GetInstance().SendMessage(peerInfo,
                                         updatedsguardidentitymessage);
    }
  }
  m_awaitingToSubmitNetworkInfoUpdate = false;

  return true;
}

bool DirectoryService::ProcessNewDSGuardNetworkInfo(
    const std::vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING,
                "Not in guard mode. Unable to update ds guard network info.");
    return false;
  }

  uint64_t epochNumber;
  Peer dsGuardNewNetworkInfo;
  uint64_t timestamp;
  PubKey dsGuardPubkey;

  if (!Messenger::GetDSLookupNewDSGuardNetworkInfo(message, offset, epochNumber,
                                                   dsGuardNewNetworkInfo,
                                                   timestamp, dsGuardPubkey)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSLookupNewDSGuardNetworkInfo failed.");
    return false;
  }

  if (m_mediator.m_selfKey.second == dsGuardPubkey) {
    LOG_GENERAL(INFO, "Node to be update is current node. No update needed.");
    return false;
  }

  uint64_t currentDSEpochNumber =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
  uint64_t loCurrentDSEpochNumber = currentDSEpochNumber - 1;
  uint64_t hiCurrentDSEpochNumber = currentDSEpochNumber + 1;

  if (!(epochNumber <= hiCurrentDSEpochNumber + 1 &&
        epochNumber >= loCurrentDSEpochNumber - 1)) {
    LOG_GENERAL(WARNING,
                "Update of ds guard network info failure due to not within "
                "range of expected "
                "ds epoch loCurrentDSEpochNumber: "
                    << loCurrentDSEpochNumber
                    << " hiCurrentDSEpochNumber: " << hiCurrentDSEpochNumber
                    << " epochNumber: " << epochNumber);
    return false;
  }

  // Check timestamp
  // Allowed only 5 mins. Else consider it obselete.
  uint64_t allowableTimeForDSNodeNetworkInfoUpdate = 300;
  if (!VerifyTimestamp(timestamp, allowableTimeForDSNodeNetworkInfoUpdate)) {
    LOG_GENERAL(WARNING, timestamp << "exceeded "
                                   << allowableTimeForDSNodeNetworkInfoUpdate
                                   << " seconds. Current time is "
                                   << get_time_as_int());
    return false;
  }

  {
    // Update DS committee
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

    unsigned int indexOfDSGuard;
    bool foundDSGuardNode = false;
    for (indexOfDSGuard = 0;
         indexOfDSGuard < Guard::GetInstance().GetNumOfDSGuard();
         indexOfDSGuard++) {
      if (m_mediator.m_DSCommittee->at(indexOfDSGuard).first == dsGuardPubkey) {
        foundDSGuardNode = true;
        LOG_GENERAL(INFO,
                    "DS guard to be updated is at index "
                        << indexOfDSGuard << " "
                        << m_mediator.m_DSCommittee->at(indexOfDSGuard).second
                        << " -> " << dsGuardNewNetworkInfo);
        m_mediator.m_DSCommittee->at(indexOfDSGuard).second =
            dsGuardNewNetworkInfo;
        break;
      }
    }

    // Lookup to store the info
    if (foundDSGuardNode &&
        m_mediator.m_DSCommittee->at(indexOfDSGuard).second !=
            dsGuardNewNetworkInfo) {
      m_mediator.m_DSCommittee->at(indexOfDSGuard).second =
          dsGuardNewNetworkInfo;
      {
        std::mutex m_mutexLookupStoreForGuardNodeUpdate;
        DSGuardUpdateStruct dsGuardNodeIden(dsGuardPubkey,
                                            dsGuardNewNetworkInfo, timestamp);
        if (m_lookupStoreForGuardNodeUpdate.find(epochNumber) ==
            m_lookupStoreForGuardNodeUpdate.end()) {
          vector<DSGuardUpdateStruct> temp = {dsGuardNodeIden};
          m_lookupStoreForGuardNodeUpdate.emplace(epochNumber, temp);
        } else {
          vector<DSGuardUpdateStruct> temp =
              m_lookupStoreForGuardNodeUpdate.at(epochNumber);
          temp.emplace_back(dsGuardNodeIden);
          m_lookupStoreForGuardNodeUpdate.at(epochNumber) = temp;
        }
      }
      return true;
    }
  }
  return false;
}

bool DirectoryService::Execute(const vector<unsigned char>& message,
                               unsigned int offset, const Peer& from) {
  // LOG_MARKER();

  bool result = false;

  typedef bool (DirectoryService::*InstructionHandler)(
      const vector<unsigned char>&, unsigned int, const Peer&);

  std::vector<InstructionHandler> ins_handlers;

  ins_handlers.insert(ins_handlers.end(),
                      {&DirectoryService::ProcessSetPrimary,
                       &DirectoryService::ProcessPoWSubmission,
                       &DirectoryService::ProcessDSBlockConsensus,
                       &DirectoryService::ProcessMicroblockSubmission,
                       &DirectoryService::ProcessFinalBlockConsensus,
                       &DirectoryService::ProcessViewChangeConsensus,
                       &DirectoryService::ProcessGetDSTxBlockMessage,
                       &DirectoryService::ProcessPoWPacketSubmission,
                       &DirectoryService::ProcessNewDSGuardNetworkInfo});

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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Unknown instruction byte " << hex << (unsigned int)ins_byte
                                          << " from " << from);
    LOG_PAYLOAD(WARNING, "Unknown payload is ", message, message.size());
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

  int64_t powSubmissions = 0;
  {
    lock_guard<mutex> g(m_mutexAllPOW);
    powSubmissions = m_allPoWs.size();
  }

  LOG_EPOCH(INFO, std::to_string(m_mediator.m_currentEpochNum).c_str(),
            "currentDifficulty "
                << std::to_string(currentDifficulty) << ", expectedNodes "
                << EXPECTED_SHARD_NODE_NUM << ", powSubmissions "
                << powSubmissions);
  return CalculateNewDifficultyCore(
      currentDifficulty, POW_DIFFICULTY, powSubmissions,
      EXPECTED_SHARD_NODE_NUM, MAX_ADJUST_THRESHOLD,
      m_mediator.m_currentEpochNum, CalculateNumberOfBlocksPerYear());
}

uint8_t DirectoryService::CalculateNewDSDifficulty(
    const uint8_t& dsDifficulty) {
  constexpr unsigned int MAX_ADJUST_THRESHOLD = 49;

  int64_t currentDSNodes = m_mediator.m_DSCommittee->size();
  int64_t dsPowSubmissions = GetNumberOfDSPoWSolns();

  LOG_EPOCH(INFO, std::to_string(m_mediator.m_currentEpochNum).c_str(),
            "dsDifficulty " << std::to_string(dsDifficulty)
                            << ", currentDSNodes " << currentDSNodes
                            << ", dsPowSubmissions " << dsPowSubmissions);

  return CalculateNewDifficultyCore(
      dsDifficulty, DS_POW_DIFFICULTY, dsPowSubmissions, currentDSNodes,
      MAX_ADJUST_THRESHOLD, m_mediator.m_currentEpochNum,
      CalculateNumberOfBlocksPerYear());
}

uint8_t DirectoryService::CalculateNewDifficultyCore(
    uint8_t currentDifficulty, uint8_t minDifficulty, int64_t powSubmissions,
    int64_t expectedNodes, uint32_t maxAdjustThreshold, int64_t currentEpochNum,
    int64_t numBlockPerYear) {
  constexpr int8_t MAX_ADJUST_STEP = 2;
  constexpr float ONE_HUNDRED_PERCENT = 100.f;
  constexpr uint8_t MAX_INCREASE_DIFFICULTY_YEARS = 10;

  int64_t adjustment = 0;
  if (expectedNodes > 0 && expectedNodes != powSubmissions) {
    int64_t submissionsDiff;
    if (!SafeMath<int64_t>::sub(powSubmissions, expectedNodes,
                                submissionsDiff)) {
      LOG_GENERAL(WARNING, "Calculate PoW submission difference goes wrong");
      submissionsDiff = 0;
    }

    // To make the adjustment work on small network.
    int64_t adjustThreshold = std::ceil(
        expectedNodes * POW_CHANGE_PERCENT_TO_ADJ_DIFF / ONE_HUNDRED_PERCENT);
    if (adjustThreshold > maxAdjustThreshold) {
      adjustThreshold = maxAdjustThreshold;
    }

    if (!SafeMath<int64_t>::div(submissionsDiff, adjustThreshold, adjustment)) {
      LOG_GENERAL(WARNING, "Calculate difficulty adjustment goes wrong");
      adjustment = 0;
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
  // upgrade the hardware over time. If POW_WINDOW_IN_SECONDS +
  // POWPACKETSUBMISSION_WINDOW_IN_SECONDS = 300, NUM_FINAL_BLOCK_PER_POW = 50,
  // TX_DISTRIBUTE_TIME_IN_MS = 10000, FINALBLOCK_DELAY_IN_MS = 3000, estimated
  // blocks in a year is 1971000.
  uint64_t estimatedBlocksOneYear =
      365 * 24 * 3600 /
      (((POW_WINDOW_IN_SECONDS + POWPACKETSUBMISSION_WINDOW_IN_SECONDS) /
        NUM_FINAL_BLOCK_PER_POW) +
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