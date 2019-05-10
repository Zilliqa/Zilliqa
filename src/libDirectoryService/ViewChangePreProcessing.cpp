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
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimestampVerifier.h"

using namespace std;

bool DirectoryService::ViewChangeValidator(
    const bytes& message, unsigned int offset, [[gnu::unused]] bytes& errorMsg,
    const uint32_t consensusID, const uint64_t blockNumber,
    const bytes& blockHash, const uint16_t leaderID, const PubKey& leaderKey,
    bytes& messageToCosign) {
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
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSVCBlockAnnouncement failed.");
    return false;
  }

  if (m_pendingVCBlock->GetHeader().GetVersion() != VCBLOCK_VERSION) {
    LOG_CHECK_FAIL("VCBlock version",
                   m_pendingVCBlock->GetHeader().GetVersion(), VCBLOCK_VERSION);
    return false;
  }

  if (!m_mediator.CheckWhetherBlockIsLatest(
          m_pendingVCBlock->GetHeader().GetViewChangeDSEpochNo(),
          m_pendingVCBlock->GetHeader().GetViewChangeEpochNo())) {
    LOG_GENERAL(WARNING,
                "ViewChangeValidator CheckWhetherBlockIsLatest failed");
    return false;
  }

  // Verify the Block Hash
  BlockHash temp_blockHash = m_pendingVCBlock->GetHeader().GetMyHash();
  if (temp_blockHash != m_pendingVCBlock->GetBlockHash()) {
    LOG_GENERAL(WARNING,
                "Block Hash in Newly received VC Block doesn't match. "
                "Calculated: "
                    << temp_blockHash
                    << " Received: " << m_pendingVCBlock->GetBlockHash().hex());
    return false;
  }

  // Check timestamp
  if (!VerifyTimestamp(m_pendingVCBlock->GetTimestamp(),
                       CONSENSUS_OBJECT_TIMEOUT)) {
    return false;
  }

  // Verify the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }
  if (committeeHash != m_pendingVCBlock->GetHeader().GetCommitteeHash()) {
    LOG_GENERAL(WARNING,
                "DS committee hash in newly received VC Block doesn't match. "
                "Calculated: "
                    << committeeHash << " Received: "
                    << m_pendingVCBlock->GetHeader().GetCommitteeHash());
    return false;
  }

  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      m_mediator.m_blocklinkchain.GetLatestBlockLink());

  if (prevHash != m_pendingVCBlock->GetHeader().GetPrevHash()) {
    LOG_GENERAL(
        WARNING,
        "Prev Block hash in newly received VC Block doesn't match. Calculated "
            << prevHash << " Received"
            << m_pendingVCBlock->GetHeader().GetPrevHash());
    return false;
  }

  // Verify candidate leader index
  uint16_t candidateLeaderIndex = CalculateNewLeaderIndex();
  if (m_mediator.m_DSCommittee->at(candidateLeaderIndex).second !=
      m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo()) {
    LOG_GENERAL(
        WARNING,
        "FATAL Candidate network info mismatched. Expected: "
            << m_mediator.m_DSCommittee->at(candidateLeaderIndex).second
            << " Obtained: "
            << m_pendingVCBlock->GetHeader().GetCandidateLeaderNetworkInfo());
    return false;
  }

  // Create a temporary local structure of ds committee and change 0.0.0.0 to
  // node's ip
  // Used range based loop due to clang tidy  enforcement
  VectorOfNode cumlativeFaultyLeaders = m_cumulativeFaultyLeaders;
  unsigned int indexToLeader = 0;
  for (const auto& node : cumlativeFaultyLeaders) {
    if (node.second == Peer()) {
      cumlativeFaultyLeaders.at(indexToLeader) =
          make_pair(cumlativeFaultyLeaders.at(indexToLeader).first,
                    m_mediator.m_selfPeer);
      break;
    }
    ++indexToLeader;
  }

  // Verify faulty leaders
  if (m_pendingVCBlock->GetHeader().GetFaultyLeaders() !=
      cumlativeFaultyLeaders) {
    LOG_GENERAL(WARNING, "View of faulty leader does not match");
    LOG_GENERAL(WARNING, "Local view of faulty leader");
    for (const auto& localFaultyLeader : cumlativeFaultyLeaders) {
      LOG_GENERAL(WARNING, "Pubkey: " << localFaultyLeader.first << " "
                                      << localFaultyLeader.second);
    }
    LOG_GENERAL(WARNING, "Proposed view of faulty leader");
    for (const auto& proposedFaultyLeader :
         m_pendingVCBlock->GetHeader().GetFaultyLeaders()) {
      LOG_GENERAL(WARNING, "Pubkey: " << proposedFaultyLeader.first << " "
                                      << proposedFaultyLeader.second);
    }
    return false;
  }

  LOG_GENERAL(INFO, "candidate leader is at index " << candidateLeaderIndex);
  for (auto& i : *m_mediator.m_DSCommittee) {
    LOG_GENERAL(INFO, i.second << " " << i.first);
  }

  if (!(m_mediator.m_DSCommittee->at(candidateLeaderIndex).first ==
        m_pendingVCBlock->GetHeader().GetCandidateLeaderPubKey())) {
    LOG_GENERAL(
        WARNING,
        "Candidate pubkey mismatched. Expected: "
            << m_mediator.m_DSCommittee->at(candidateLeaderIndex).first
            << " Obtained: "
            << m_pendingVCBlock->GetHeader().GetCandidateLeaderPubKey());
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
    LOG_GENERAL(WARNING,
                "View change counter mismatched. Expected: "
                    << m_viewChangeCounter << " Obtained: "
                    << m_pendingVCBlock->GetHeader().GetViewChangeCounter());
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

  SetLastKnownGoodState();
  SetState(VIEWCHANGE_CONSENSUS_PREP);

  uint64_t dsCurBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t txCurBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  // Note: Special check as 0 and 1 have special usage when fetching ds block
  // and final block No need check for 1 as
  // VCFetchLatestDSTxBlockFromSeedNodes always check for current block + 1
  // i.e in first epoch, it will request for block 1, which means fetch latest
  // block (including block 0)
  if (dsCurBlockNum != 0 && txCurBlockNum != 0) {
    VCFetchLatestDSTxBlockFromSeedNodes();
    if (!NodeVCPrecheck()) {
      LOG_GENERAL(WARNING,
                  "[RDS]Failed the vc precheck. Node is lagging behind the "
                  "whole network.");
      CleanUpViewChange(true);
      RejoinAsDS();
      return;
    }
  }

  // Blacklist::GetInstance().Clear();

  uint16_t faultyLeaderIndex;
  m_viewChangeCounter += 1;
  if (m_viewChangeCounter == 1) {
    faultyLeaderIndex = GetConsensusLeaderID();
  } else {
    faultyLeaderIndex = m_candidateLeaderIndex;
  }

  bool ConsensusObjCreation = true;

  {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    // Ensure that one does not emplace 0.0.0.0
    if (m_mediator.m_DSCommittee->at(faultyLeaderIndex).first ==
            m_mediator.m_selfKey.second &&
        m_mediator.m_DSCommittee->at(faultyLeaderIndex).second == Peer()) {
      m_cumulativeFaultyLeaders.emplace_back(
          m_mediator.m_DSCommittee->at(faultyLeaderIndex).first,
          m_mediator.m_selfPeer);
    } else {
      m_cumulativeFaultyLeaders.emplace_back(
          m_mediator.m_DSCommittee->at(faultyLeaderIndex));
    }

    m_candidateLeaderIndex = CalculateNewLeaderIndex();

    LOG_GENERAL(
        INFO,
        "The new consensus leader is at index "
            << to_string(m_candidateLeaderIndex) << " "
            << m_mediator.m_DSCommittee->at(m_candidateLeaderIndex).second);

    if (DEBUG_LEVEL >= 5) {
      for (auto& i : *m_mediator.m_DSCommittee) {
        LOG_GENERAL(INFO, i.second);
      }
    }

    // Upon consensus object creation failure, one should not return from the
    // function, but rather wait for view change.
    // We compare with empty peer is due to the fact that DSCommittee for
    // yourself is 0.0.0.0 with port 0.
    if (m_mediator.m_DSCommittee->at(m_candidateLeaderIndex).second == Peer()) {
      ConsensusObjCreation =
          RunConsensusOnViewChangeWhenCandidateLeader(m_candidateLeaderIndex);
      if (!ConsensusObjCreation) {
        LOG_GENERAL(WARNING, "Error after RunConsensusOnDSBlockWhenDSPrimary");
      }
    } else {
      ConsensusObjCreation = RunConsensusOnViewChangeWhenNotCandidateLeader(
          m_candidateLeaderIndex);
      if (!ConsensusObjCreation) {
        LOG_GENERAL(WARNING,
                    "Error after "
                    "RunConsensusOnViewChangeWhenNotCandidateLeader");
      }
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
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Initiated view change again");

    if (m_mode == PRIMARY_DS) {
      ConsensusLeader* cl =
          dynamic_cast<ConsensusLeader*>(m_consensusObject.get());
      if (cl != nullptr) {
        cl->Audit();
      }
    }

    auto func = [this]() -> void { RunConsensusOnViewChange(); };
    DetachedFunction(1, func);
  }
}

bool DirectoryService::ComputeNewCandidateLeader(
    const uint16_t candidateLeaderIndex) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComputeNewCandidateLeader not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  // Assemble VC block header

  Peer newLeaderNetworkInfo;
  if (m_mediator.m_DSCommittee->at(candidateLeaderIndex).first ==
          m_mediator.m_selfKey.second &&
      m_mediator.m_DSCommittee->at(candidateLeaderIndex).second == Peer()) {
    // I am the leader but in the Peer store, it is put as 0.0.0.0 with port 0
    newLeaderNetworkInfo = m_mediator.m_selfPeer;
  } else {
    newLeaderNetworkInfo =
        m_mediator.m_DSCommittee->at(candidateLeaderIndex).second;
  }

  LOG_GENERAL(INFO,
              "Composing new vc block with vc count at "
                  << m_viewChangeCounter << " and candidate leader is at index "
                  << candidateLeaderIndex << ". " << newLeaderNetworkInfo << " "
                  << m_mediator.m_DSCommittee->at(candidateLeaderIndex).first);

  // Compute the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }
  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      m_mediator.m_blocklinkchain.GetLatestBlockLink());
  {
    lock_guard<mutex> g(m_mutexPendingVCBlock);
    // To-do: Handle exceptions.
    m_pendingVCBlock.reset(new VCBlock(
        VCBlockHeader(
            m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
                1,
            m_mediator.m_currentEpochNum, m_viewChangestate,
            newLeaderNetworkInfo,
            m_mediator.m_DSCommittee->at(candidateLeaderIndex).first,
            m_viewChangeCounter, m_cumulativeFaultyLeaders, VCBLOCK_VERSION,
            committeeHash, prevHash),
        CoSignatures()));
  }

  return true;
}

bool DirectoryService::NodeVCPrecheck() {
  LOG_MARKER();
  {
    lock_guard<mutex> g(m_MutexCVViewChangePrecheckBlocks);
    m_vcPreCheckDSBlocks.clear();
    m_vcPreCheckTxBlocks.clear();
  }

  std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangePrecheck);
  if (cv_viewChangePrecheck.wait_for(
          cv_lk, std::chrono::seconds(VIEWCHANGE_PRECHECK_TIME)) ==
      std::cv_status::timeout) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Timeout while waiting for precheck. ");
  }

  {
    lock_guard<mutex> g(m_MutexCVViewChangePrecheckBlocks);
    if (m_vcPreCheckDSBlocks.size() == 0 && m_vcPreCheckTxBlocks.size() == 0) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Passed precheck. ");
      return true;
    }
  }
  LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
            "Failed precheck. m_vcPreCheckDSBlocks size: "
                << m_vcPreCheckDSBlocks.size() << " m_vcPreCheckTxBlocks size: "
                << m_vcPreCheckTxBlocks.size());
  return false;
}

uint16_t DirectoryService::CalculateNewLeaderIndex() {
  // New leader is computed using the following
  // new candidate leader index is
  // H((finalblock or vc block), vc counter) % size
  // of ds committee
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  uint64_t latestIndex = m_mediator.m_blocklinkchain.GetLatestIndex();
  BlockLink bl = m_mediator.m_blocklinkchain.GetBlockLink(latestIndex);
  VCBlockSharedPtr prevVCBlockptr;
  if (CheckUseVCBlockInsteadOfDSBlock(bl, prevVCBlockptr)) {
    LOG_GENERAL(INFO,
                "Using hash of last vc block for computing candidate leader");
    sha2.Update(prevVCBlockptr->GetBlockHash().asBytes());
  } else {
    LOG_GENERAL(
        INFO, "Using hash of last final block for computing candidate leader");
    sha2.Update(
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes());
  }

  bytes vcCounterBytes;
  Serializable::SetNumber<uint32_t>(vcCounterBytes, 0, m_viewChangeCounter,
                                    sizeof(uint32_t));
  sha2.Update(vcCounterBytes);
  uint16_t lastBlockHash = DataConversion::charArrTo16Bits(sha2.Finalize());
  uint16_t candidateLeaderIndex;

  if (!GUARD_MODE) {
    candidateLeaderIndex = lastBlockHash % m_mediator.m_DSCommittee->size();
  } else {
    candidateLeaderIndex =
        lastBlockHash % Guard::GetInstance().GetNumOfDSGuard();
  }

  while (candidateLeaderIndex == GetConsensusLeaderID()) {
    LOG_GENERAL(INFO,
                "Computed candidate leader is current faulty ds leader. Index: "
                    << candidateLeaderIndex);
    sha2.Update(sha2.Finalize());
    lastBlockHash = DataConversion::charArrTo16Bits(sha2.Finalize());
    if (!GUARD_MODE) {
      candidateLeaderIndex = lastBlockHash % m_mediator.m_DSCommittee->size();
    } else {
      candidateLeaderIndex =
          lastBlockHash % Guard::GetInstance().GetNumOfDSGuard();
      LOG_GENERAL(INFO, "In Guard mode. interim candidate leader is "
                            << candidateLeaderIndex);
    }

    LOG_GENERAL(INFO, "Re-computed candidate leader is at index: "
                          << candidateLeaderIndex
                          << " VC counter: " << m_viewChangeCounter);
  }
  return candidateLeaderIndex;
}

bool DirectoryService::CheckUseVCBlockInsteadOfDSBlock(
    const BlockLink& bl, VCBlockSharedPtr& prevVCBlockptr) {
  BlockType latestBlockType = get<BlockLinkIndex::BLOCKTYPE>(bl);

  if (latestBlockType == BlockType::VC) {
    if (!BlockStorage::GetBlockStorage().GetVCBlock(
            get<BlockLinkIndex::BLOCKHASH>(bl), prevVCBlockptr)) {
      LOG_GENERAL(WARNING, "could not get vc block "
                               << get<BlockLinkIndex::BLOCKHASH>(bl));
      return false;
    }

    if (prevVCBlockptr->GetHeader().GetViewChangeEpochNo() !=
        m_mediator.m_currentEpochNum) {
      return false;
    }

    if (m_viewChangestate == DSBLOCK_CONSENSUS ||
        m_viewChangestate == DSBLOCK_CONSENSUS_PREP) {
      if (prevVCBlockptr->GetHeader().GetViewChangeState() ==
              DSBLOCK_CONSENSUS ||
          prevVCBlockptr->GetHeader().GetViewChangeState() ==
              DSBLOCK_CONSENSUS_PREP) {
        return true;
      } else {
        LOG_GENERAL(
            WARNING,
            "The previous vc block is not for current state.  prevVCBlockptr: "
                << to_string(prevVCBlockptr->GetHeader().GetViewChangeState())
                << " m_viewChangestate:" << m_viewChangestate);
        return false;
      }
    }

    if (m_viewChangestate == FINALBLOCK_CONSENSUS ||
        m_viewChangestate == FINALBLOCK_CONSENSUS_PREP) {
      if (prevVCBlockptr->GetHeader().GetViewChangeState() ==
              FINALBLOCK_CONSENSUS ||
          prevVCBlockptr->GetHeader().GetViewChangeState() ==
              FINALBLOCK_CONSENSUS_PREP) {
        return true;
      } else {
        LOG_GENERAL(
            WARNING,
            "The previous vc block is not for current state.  prevVCBlockptr: "
                << to_string(prevVCBlockptr->GetHeader().GetViewChangeState())
                << " m_viewChangestate:" << m_viewChangestate);
        return false;
      }
    }
  } else {
    return false;
  }
  return true;
}

bool DirectoryService::RunConsensusOnViewChangeWhenCandidateLeader(
    const uint16_t candidateLeaderIndex) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::"
                "RunConsensusOnViewChangeWhenCandidateLeader not expected "
                "to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

#ifdef VC_TEST_VC_SUSPEND_1
  if (m_viewChangeCounter < 2) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_VC_SUSPEND_1)");
    return false;
  }
#endif  // VC_TEST_VC_SUSPEND_1

#ifdef VC_TEST_VC_SUSPEND_3
  if (m_viewChangeCounter < 4) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_VC_SUSPEND_3)");
    return false;
  }
#endif  // VC_TEST_VC_SUSPEND_3

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am the candidate leader DS node. Announcing to the rest.");

  if (!ComputeNewCandidateLeader(candidateLeaderIndex)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "DirectoryService::ComputeNewCandidateLeader failed");
    return false;
  }

  uint32_t consensusID = m_viewChangeCounter;
  // Create new consensus object
  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  m_consensusObject.reset(new ConsensusLeader(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
      static_cast<uint8_t>(DIRECTORY),
      static_cast<uint8_t>(VIEWCHANGECONSENSUS), NodeCommitFailureHandlerFunc(),
      ShardCommitFailureHandlerFunc(), true));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Error: Unable to create consensus object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  bytes m;
  {
    lock_guard<mutex> g(m_mutexPendingVCBlock);
    m_pendingVCBlock->Serialize(m, 0);
  }

  std::this_thread::sleep_for(std::chrono::seconds(VIEWCHANGE_EXTRA_TIME));

  auto announcementGeneratorFunc =
      [this](bytes& dst, unsigned int offset, const uint32_t consensusID,
             const uint64_t blockNumber, const bytes& blockHash,
             const uint16_t leaderID, const PairOfKey& leaderKey,
             bytes& messageToCosign) mutable -> bool {
    lock_guard<mutex> g(m_mutexPendingVCBlock);
    return Messenger::SetDSVCBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_pendingVCBlock, messageToCosign);
  };

  cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);

  return true;
}

bool DirectoryService::RunConsensusOnViewChangeWhenNotCandidateLeader(
    const uint16_t candidateLeaderIndex) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::"
                "RunConsensusOnViewChangeWhenNotCandidateLeader not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am a backup DS node (after view change). Waiting for view "
            "change announcement. "
            "Leader is at index  "
                << candidateLeaderIndex << " "
                << m_mediator.m_DSCommittee->at(candidateLeaderIndex).second);

  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  auto func = [this](const bytes& input, unsigned int offset, bytes& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const bytes& blockHash, const uint16_t leaderID,
                     const PubKey& leaderKey,
                     bytes& messageToCosign) mutable -> bool {
    return ViewChangeValidator(input, offset, errorMsg, consensusID,
                               blockNumber, blockHash, leaderID, leaderKey,
                               messageToCosign);
  };

  uint32_t consensusID = m_viewChangeCounter;
  m_consensusObject.reset(new ConsensusBackup(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, candidateLeaderIndex, m_mediator.m_selfKey.first,
      *m_mediator.m_DSCommittee, static_cast<uint8_t>(DIRECTORY),
      static_cast<uint8_t>(VIEWCHANGECONSENSUS), func));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Error: Unable to create consensus object");
    return false;
  }

  return true;
}

bool DirectoryService::VCFetchLatestDSTxBlockFromSeedNodes() {
  LOG_MARKER();
  m_mediator.m_lookup->SendMessageToRandomSeedNode(
      ComposeVCGetDSTxBlockMessage());
  return true;
}

bytes DirectoryService::ComposeVCGetDSTxBlockMessage() {
  LOG_MARKER();
  bytes getDSTxBlockMessage = {MessageType::LOOKUP,
                               LookupInstructionType::VCGETLATESTDSTXBLOCK};
  uint64_t dslowBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
  uint64_t txlowBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
  if (!Messenger::SetLookupGetDSTxBlockFromSeed(
          getDSTxBlockMessage, MessageOffset::BODY, dslowBlockNum, 0,
          txlowBlockNum, 0, m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetDSTxBlockFromSeed failed.");
    return {};
  }
  LOG_GENERAL(INFO, "Checking for new blocks. new (if any) dslowBlockNum: "
                        << dslowBlockNum
                        << " new (if any) txlowBlockNum: " << txlowBlockNum);

  return getDSTxBlockMessage;
}

bool DirectoryService::ProcessGetDSTxBlockMessage(
    const bytes& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessGetDSTxBlockMessage not expected "
                "to be called from LookUp node.");
    return true;
  }

  if (m_state != VIEWCHANGE_CONSENSUS_PREP) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "Unable to process ProcessGetDSTxBlockMessage as current state is "
            << to_string(m_state));
  }

  lock_guard<mutex> g(m_MutexCVViewChangePrecheckBlocks);

  PubKey lookupPubKey;
  vector<DSBlock> vcPreCheckDSBlocks;
  vector<TxBlock> vcPreCheckTxBlocks;
  if (!Messenger::GetVCNodeSetDSTxBlockFromSeed(
          message, offset, vcPreCheckDSBlocks, vcPreCheckTxBlocks,
          lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetVCNodeSetDSTxBlockFromSeed failed.");
    return false;
  }

  if (!m_mediator.m_lookup->VerifySenderNode(
          m_mediator.m_lookup->GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  m_vcPreCheckDSBlocks = vcPreCheckDSBlocks;
  m_vcPreCheckTxBlocks = vcPreCheckTxBlocks;

  cv_viewChangePrecheck.notify_all();
  return true;
}
