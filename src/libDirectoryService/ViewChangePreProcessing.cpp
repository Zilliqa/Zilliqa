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

using namespace std;

bool DirectoryService::ViewChangeValidator(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] vector<unsigned char>& errorMsg, const uint32_t consensusID,
    const uint64_t blockNumber, const vector<unsigned char>& blockHash,
    const uint16_t leaderID, const PubKey& leaderKey,
    vector<unsigned char>& messageToCosign) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ViewChangeValidator not expected to be "
                "called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  lock_guard<mutex> g(m_mutexPendingVCBlock);

  m_pendingVCBlock.reset(new VCBlock);

  if (!Messenger::GetDSVCBlockAnnouncement(
          message, offset, consensusID, blockNumber, blockHash, leaderID,
          leaderKey, *m_pendingVCBlock, messageToCosign)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSVCBlockAnnouncement failed.");
    return false;
  }

  if (m_mediator.m_DSCommittee->at(m_viewChangeCounter).second !=
      m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo()) {
    LOG_GENERAL(WARNING, "Candidate network info mismatched");
    return false;
  }

  if (!(m_mediator.m_DSCommittee->at(m_viewChangeCounter).first ==
        m_pendingVCBlock->GetHeader().GetCandidateLeaderPubKey())) {
    LOG_GENERAL(WARNING, "Candidate pubkey mismatched");
    return false;
  }

  if (!ValidateViewChangeState(
          m_viewChangestate,
          (DirState)m_pendingVCBlock->GetHeader().GetViewChangeState())) {
    LOG_GENERAL(
        WARNING,
        "View change state mismatched. m_viewChangestate: "
            << m_viewChangestate << " Proposed: "
            << (DirState)m_pendingVCBlock->GetHeader().GetViewChangeState());
    return false;
  }

  if (m_viewChangeCounter !=
      m_pendingVCBlock->GetHeader().GetViewChangeCounter()) {
    LOG_GENERAL(WARNING, "View change counter mismatched");
    return false;
  }
  return true;
}

bool DirectoryService::ValidateViewChangeState(DirState NodeState,
                                               DirState StatePropose) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ValidateViewChangeState not expected to "
                "be called from LookUp node.");
    return true;
  }

  const std::multimap<DirState, DirState> STATE_CHECK_STATE = {
      {DSBLOCK_CONSENSUS_PREP, DSBLOCK_CONSENSUS_PREP},
      {DSBLOCK_CONSENSUS_PREP, DSBLOCK_CONSENSUS},
      {DSBLOCK_CONSENSUS, DSBLOCK_CONSENSUS_PREP},
      {DSBLOCK_CONSENSUS, DSBLOCK_CONSENSUS},
      {FINALBLOCK_CONSENSUS_PREP, FINALBLOCK_CONSENSUS_PREP},
      {FINALBLOCK_CONSENSUS_PREP, FINALBLOCK_CONSENSUS},
      {FINALBLOCK_CONSENSUS, FINALBLOCK_CONSENSUS_PREP},
      {FINALBLOCK_CONSENSUS, FINALBLOCK_CONSENSUS}};

  for (auto pos = STATE_CHECK_STATE.lower_bound(NodeState);
       pos != STATE_CHECK_STATE.upper_bound(NodeState); pos++) {
    if (pos->second == StatePropose) {
      return true;
    }
  }
  return false;
}

// The idea of this function is to set the last know good state of the network
// before view change happens. This allows for the network to resume from where
// it left.
void DirectoryService::SetLastKnownGoodState() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SetLastKnownGoodState not expected to "
                "be called from LookUp node.");
    return;
  }

  switch (m_state) {
    case VIEWCHANGE_CONSENSUS_PREP:
    case VIEWCHANGE_CONSENSUS:
    case ERROR:
      break;
    default:
      m_viewChangestate = (DirState)m_state;
  }
}

void DirectoryService::RunConsensusOnViewChange() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnViewChange not expected "
                "to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  LOG_GENERAL(WARNING, "Run view change, revert state delta");
  AccountStore::GetInstance().InitTemp();
  AccountStore::GetInstance().DeserializeDeltaTemp(
      m_mediator.m_ds->m_stateDeltaWhenRunDSMB, 0);
  AccountStore::GetInstance().RevertCommitTemp();

  SetLastKnownGoodState();
  SetState(VIEWCHANGE_CONSENSUS_PREP);

  m_viewChangeCounter =
      (m_viewChangeCounter + 1) %
      m_mediator.m_DSCommittee
          ->size();  // TODO: To be change to a random node using VRF

  LOG_GENERAL(INFO, "The new consensus leader is at index "
                        << to_string(m_viewChangeCounter));

  for (auto& i : *m_mediator.m_DSCommittee) {
    LOG_GENERAL(INFO, i.second);
  }

  // Upon consensus object creation failure, one should not return from the
  // function, but rather wait for view change.
  bool ConsensusObjCreation = true;

  // We compare with empty peer is due to the fact that DSCommittee for yourself
  // is 0.0.0.0 with port 0.
  if (m_mediator.m_DSCommittee->at(m_viewChangeCounter).second == Peer()) {
    ConsensusObjCreation = RunConsensusOnViewChangeWhenCandidateLeader();
    if (!ConsensusObjCreation) {
      LOG_GENERAL(WARNING, "Error after RunConsensusOnDSBlockWhenDSPrimary");
    }
  } else {
    ConsensusObjCreation = RunConsensusOnViewChangeWhenNotCandidateLeader();
    if (!ConsensusObjCreation) {
      LOG_GENERAL(WARNING,
                  "Error after "
                  "RunConsensusOnViewChangeWhenNotCandidateLeader");
    }
  }

  if (ConsensusObjCreation) {
    SetState(VIEWCHANGE_CONSENSUS);
    cv_ViewChangeConsensusObj.notify_all();
  }

  auto func = [this]() -> void { ScheduleViewChangeTimeout(); };
  DetachedFunction(1, func);
}

void DirectoryService::ScheduleViewChangeTimeout() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ScheduleViewChangeTimeout not expected "
                "to be called from LookUp node.");
    return;
  }

  std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeVCBlock);
  if (cv_ViewChangeVCBlock.wait_for(cv_lk,
                                    std::chrono::seconds(VIEWCHANGE_TIME)) ==
      std::cv_status::timeout) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Initiated view change again");

    auto func = [this]() -> void { RunConsensusOnViewChange(); };
    DetachedFunction(1, func);
  }
}

void DirectoryService::ComputeNewCandidateLeader() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComputeNewCandidateLeader not expected "
                "to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  // Assemble VC block header

  LOG_GENERAL(
      INFO, "Composing new vc block with vc count at " << m_viewChangeCounter);

  Peer newLeaderNetworkInfo;
  if (m_mediator.m_DSCommittee->at(m_viewChangeCounter).second == Peer()) {
    // I am the leader but in the Peer store, it is put as 0.0.0.0 with port 0
    newLeaderNetworkInfo = m_mediator.m_selfPeer;
  } else {
    newLeaderNetworkInfo =
        m_mediator.m_DSCommittee->at(m_viewChangeCounter).second;
  }

  {
    lock_guard<mutex> g(m_mutexPendingVCBlock);
    // To-do: Handle exceptions.
    m_pendingVCBlock.reset(new VCBlock(
        VCBlockHeader(
            m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
                1,
            m_mediator.m_currentEpochNum, m_viewChangestate,
            m_viewChangeCounter, newLeaderNetworkInfo,
            m_mediator.m_DSCommittee->at(m_viewChangeCounter).first,
            m_viewChangeCounter, get_time_as_int()),
        CoSignatures()));
  }
}

bool DirectoryService::RunConsensusOnViewChangeWhenCandidateLeader() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::"
                "RunConsensusOnViewChangeWhenCandidateLeader not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  // view change testing code (for failure of candidate leader)
  /**
  if (true && m_viewChangeCounter < 2)
  {
      LOG_EPOCH(
          WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
          "I am killing/suspending myself to test recursive view change.");
      throw Exception();
      // return false;
  }
  **/

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I am the candidate leader DS node. Announcing to the rest.");

  ComputeNewCandidateLeader();

  uint32_t consensusID = m_viewChangeCounter;
  // Create new consensus object
  m_consensusBlockHash = m_mediator.m_txBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetMyHash()
                             .asBytes();

  m_consensusObject.reset(new ConsensusLeader(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
      static_cast<unsigned char>(DIRECTORY),
      static_cast<unsigned char>(VIEWCHANGECONSENSUS),
      NodeCommitFailureHandlerFunc(), ShardCommitFailureHandlerFunc()));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Error: Unable to create consensus object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  vector<unsigned char> m;
  {
    lock_guard<mutex> g(m_mutexPendingVCBlock);
    m_pendingVCBlock->Serialize(m, 0);
  }

  std::this_thread::sleep_for(std::chrono::seconds(VIEWCHANGE_EXTRA_TIME));

  auto announcementGeneratorFunc =
      [this](vector<unsigned char>& dst, unsigned int offset,
             const uint32_t consensusID, const uint64_t blockNumber,
             const vector<unsigned char>& blockHash, const uint16_t leaderID,
             const pair<PrivKey, PubKey>& leaderKey,
             vector<unsigned char>& messageToCosign) mutable -> bool {
    lock_guard<mutex> g(m_mutexPendingVCBlock);
    return Messenger::SetDSVCBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_pendingVCBlock, messageToCosign);
  };

  cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);

  return true;
}

bool DirectoryService::RunConsensusOnViewChangeWhenNotCandidateLeader() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::"
                "RunConsensusOnViewChangeWhenNotCandidateLeader not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I am a backup DS node (after view change). Waiting for view "
            "change announcement. "
            "Leader is at index  "
                << m_consensusLeaderID << " "
                << m_mediator.m_DSCommittee->at(m_consensusLeaderID).second);

  m_consensusBlockHash = m_mediator.m_txBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetMyHash()
                             .asBytes();

  auto func = [this](const vector<unsigned char>& input, unsigned int offset,
                     vector<unsigned char>& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const vector<unsigned char>& blockHash,
                     const uint16_t leaderID, const PubKey& leaderKey,
                     vector<unsigned char>& messageToCosign) mutable -> bool {
    return ViewChangeValidator(input, offset, errorMsg, consensusID,
                               blockNumber, blockHash, leaderID, leaderKey,
                               messageToCosign);
  };

  uint32_t consensusID = m_viewChangeCounter;
  m_consensusObject.reset(new ConsensusBackup(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, m_viewChangeCounter, m_mediator.m_selfKey.first,
      *m_mediator.m_DSCommittee, static_cast<unsigned char>(DIRECTORY),
      static_cast<unsigned char>(VIEWCHANGECONSENSUS), func));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Error: Unable to create consensus object");
    return false;
  }

  return true;
}
