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

#include "ConsensusLeader.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

using namespace std;

bool ConsensusLeader::CheckState(Action action) {
  static const std::multimap<ConsensusCommon::State, Action> ACTIONS_FOR_STATE =
      {{INITIAL, SEND_ANNOUNCEMENT},
       {INITIAL, PROCESS_COMMITFAILURE},
       {ANNOUNCE_DONE, PROCESS_COMMIT},
       {ANNOUNCE_DONE, PROCESS_COMMITFAILURE},
       {CHALLENGE_DONE, PROCESS_RESPONSE},
       {CHALLENGE_DONE, PROCESS_COMMITFAILURE},
       {COLLECTIVESIG_DONE, PROCESS_FINALCOMMIT},
       {COLLECTIVESIG_DONE, PROCESS_COMMITFAILURE},
       {FINALCHALLENGE_DONE, PROCESS_FINALRESPONSE},
       {FINALCHALLENGE_DONE, PROCESS_COMMITFAILURE},
       {DONE, PROCESS_COMMITFAILURE}};

  bool found = false;

  for (auto pos = ACTIONS_FOR_STATE.lower_bound(m_state);
       pos != ACTIONS_FOR_STATE.upper_bound(m_state); pos++) {
    if (pos->second == action) {
      found = true;
      break;
    }
  }

  if (!found) {
    LOG_GENERAL(WARNING, GetActionString(action)
                             << " not allowed in " << GetStateString());
    return false;
  }

  return true;
}

bool ConsensusLeader::CheckStateSubset(uint16_t subsetID, Action action) {
  ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

  static const std::multimap<ConsensusCommon::State, Action> ACTIONS_FOR_STATE =
      {{CHALLENGE_DONE, PROCESS_RESPONSE},
       {FINALCHALLENGE_DONE, PROCESS_FINALRESPONSE}};

  bool found = false;

  for (auto pos = ACTIONS_FOR_STATE.lower_bound(subset.state);
       pos != ACTIONS_FOR_STATE.upper_bound(subset.state); pos++) {
    if (pos->second == action) {
      found = true;
      break;
    }
  }

  if (!found) {
    LOG_GENERAL(WARNING, "[Subset " << subsetID << "] "
                                    << GetActionString(action)
                                    << " not allowed in subset-state "
                                    << GetStateString(subset.state)
                                    << ", overall state: " << GetStateString());
    return false;
  }

  return true;
}

void ConsensusLeader::SetStateSubset(uint16_t subsetID, State newState) {
  LOG_MARKER();
  if ((newState == INITIAL) ||
      (newState > m_consensusSubsets.at(subsetID).state)) {
    m_consensusSubsets.at(subsetID).state = newState;
  }
}

void ConsensusLeader::GenerateConsensusSubsets() {
  LOG_MARKER();

  // Get the list of all the peers who committed, by peer index
  vector<unsigned int> peersWhoCommitted;
  for (unsigned int index = 0; index < m_commitMap.size(); index++) {
    if (m_commitMap.at(index) && index != m_myID) {
      peersWhoCommitted.push_back(index);
    }
  }
  // Generate m_numOfSubsets lists (= subsets of peersWhoCommitted)
  // If we have exactly the minimum num required for consensus, no point making
  // more than 1 subset

  const unsigned int numSubsets =
      (peersWhoCommitted.size() <= m_numForConsensus) ? 1 : m_numOfSubsets;
  LOG_GENERAL(INFO, "peersWhoCommitted = " << peersWhoCommitted.size() + 1);
  LOG_GENERAL(INFO, "m_numForConsensus = " << m_numForConsensus);
  LOG_GENERAL(INFO, "numSubsets        = " << numSubsets);

  m_consensusSubsets.clear();
  m_consensusSubsets.resize(numSubsets);

  for (unsigned int i = 0; i < numSubsets; i++) {
    ConsensusSubset& subset = m_consensusSubsets.at(i);
    subset.commitMap.resize(m_committee.size());
    fill(subset.commitMap.begin(), subset.commitMap.end(), false);
    subset.commitPointMap.resize(m_committee.size());
    subset.commitPoints.clear();
    subset.responseCounter = 0;
    subset.responseDataMap.resize(m_committee.size());
    subset.responseMap.resize(m_committee.size());
    fill(subset.responseMap.begin(), subset.responseMap.end(), false);
    subset.responseData.clear();

    subset.state = m_state;
    // add myself to subset commit map always
    subset.commitPointMap.at(m_myID) = m_commitPointMap.at(m_myID);
    subset.commitPoints.emplace_back(m_commitPointMap.at(m_myID));
    subset.commitMap.at(m_myID) = true;

    // If DS consensus, then first subset should be of dsguard commits only.
    // Fill in from rest if commits from dsguards < m_numForConsensus
    if (m_DS && GUARD_MODE && (i == 0)) {
      unsigned int subsetPeers = 1;  // myself
      vector<unsigned int> nondsguardIndexes;
      for (auto index : peersWhoCommitted) {
        if (index < Guard::GetInstance().GetNumOfDSGuard()) {
          subset.commitPointMap.at(index) = m_commitPointMap.at(index);
          subset.commitPoints.emplace_back(m_commitPointMap.at(index));
          subset.commitMap.at(index) = true;
          subsetPeers++;
          if (subsetPeers == m_numForConsensus) {
            // got all dsguards commit
            LOG_GENERAL(INFO, "[SubsetID: " << i << "] Got all "
                                            << m_numForConsensus
                                            << " commits from ds-guards");
            break;
          }
        } else {
          nondsguardIndexes.push_back(index);
        }
      }

      // check if we fall short of commits from dsguards
      if (subsetPeers < m_numForConsensus) {
        // Add from rest of nondsguards commits
        LOG_GENERAL(WARNING, "[SubsetID: " << i << "] Guards = " << subsetPeers
                                           << ", Non-guards = "
                                           << m_numForConsensus - subsetPeers);

        for (auto index : nondsguardIndexes) {
          subset.commitPointMap.at(index) = m_commitPointMap.at(index);
          subset.commitPoints.emplace_back(m_commitPointMap.at(index));
          subset.commitMap.at(index) = true;
          if (++subsetPeers >= m_numForConsensus) {
            break;
          }
        }
      }
    }
    // For other subsets, its commit from every one together.
    else {
      for (unsigned int j = 0; j < m_numForConsensus - 1; j++) {
        unsigned int index = peersWhoCommitted.at(j);
        subset.commitPointMap.at(index) = m_commitPointMap.at(index);
        subset.commitPoints.emplace_back(m_commitPointMap.at(index));
        subset.commitMap.at(index) = true;
      }
    }

    if (DEBUG_LEVEL >= 5) {
      LOG_GENERAL(INFO, "SubsetID: " << i);
      for (unsigned int k = 0; k < subset.commitMap.size(); k++) {
        LOG_GENERAL(INFO,
                    "Commit map " << k << " = " << subset.commitMap.at(k));
      }
    }

    random_shuffle(peersWhoCommitted.begin(), peersWhoCommitted.end());
  }
  // Clear out the original commit map stuff, we don't need it anymore at this
  // point
  m_commitPointMap.clear();
  m_commitPoints.clear();
  m_commitMap.clear();
}

bool ConsensusLeader::StartConsensusSubsets() {
  LOG_MARKER();

  ConsensusMessageType type = ConsensusMessageType::CHALLENGE;
  // Update overall internal state
  if (m_state == ANNOUNCE_DONE) {
    m_state = CHALLENGE_DONE;
    type = ConsensusMessageType::CHALLENGE;
  } else if (m_state == COLLECTIVESIG_DONE) {
    m_state = FINALCHALLENGE_DONE;
    type = ConsensusMessageType::FINALCHALLENGE;
  } else {
    LOG_GENERAL(WARNING, "Wrong state");
    return false;
  }

  m_numSubsetsRunning = m_consensusSubsets.size();

  bytes challenge = {m_classByte, m_insByte, static_cast<uint8_t>(type)};
  if (!GenerateChallengeMessage(challenge,
                                MessageOffset::BODY + sizeof(uint8_t))) {
    LOG_GENERAL(WARNING, "GenerateChallengeMessage failed");
    m_state = ERROR;
    return false;
  }

  for (unsigned int index = 0; index < m_consensusSubsets.size(); index++) {
    ConsensusSubset& subset = m_consensusSubsets.at(index);

    // Update subset's internal state
    SetStateSubset(index, m_state);

    // Add the leader to the responses
    Response r(*m_commitSecret, subset.challenge, m_myPrivKey);
    subset.responseData.emplace_back(r);
    subset.responseDataMap.at(m_myID) = r;
    subset.responseMap.at(m_myID) = true;
    subset.responseCounter = 1;
  }

  // Multicast challenge to everyone who belongs to at least one of the subsets
  deque<Peer> peerInfo;
  for (unsigned int i = 0; i < m_committee.size(); i++) {
    for (const auto& subset : m_consensusSubsets) {
      if (subset.commitMap.at(i)) {
        peerInfo.push_back(m_committee.at(i).second);
        break;
      }
    }
  }

  // Shuffle the peer list so we don't always send challenges in same sequence
  random_shuffle(peerInfo.begin(), peerInfo.end());

  P2PComm::GetInstance().SendMessage(peerInfo, challenge);

  return true;
}

void ConsensusLeader::LogResponsesStats(unsigned int subsetID) {
  if (m_DS && GUARD_MODE) {
    LOG_MARKER();
    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);
    unsigned int dsguardCount = 0, nondsguardCount = 0;
    for (unsigned int i = 0; i < subset.responseMap.size(); i++) {
      if (subset.responseMap[i]) {
        if (i < Guard::GetInstance().GetNumOfDSGuard()) {
          dsguardCount++;
        } else {
          nondsguardCount++;
        }
      }
    }
    LOG_GENERAL(
        INFO, "[SubsetID: " << subsetID
                            << "] Responses received: Guards = " << dsguardCount
                            << ", Non-guards = " << nondsguardCount);
  }
}

void ConsensusLeader::SubsetEnded(uint16_t subsetID) {
  LOG_MARKER();
  ConsensusSubset& subset = m_consensusSubsets.at(subsetID);
  if (subset.state == COLLECTIVESIG_DONE || subset.state == DONE) {
    // We've achieved consensus!
    LOG_GENERAL(INFO, "[Subset " << subsetID << "] Subset DONE");
    // Reset all other subsets to INITIAL so they reject any further messages
    // from their backups
    for (unsigned int i = 0; i < m_consensusSubsets.size(); i++) {
      // Log the responses stats if its ds consensus and DS_GUARD mode
      LogResponsesStats(i);

      if (i == subsetID) {
        continue;
      }
      SetStateSubset(i, INITIAL);
    }
    // Set overall state to that of subset i.e. COLLECTIVESIG_DONE OR DONE
    m_state = subset.state;
  } else if (--m_numSubsetsRunning == 0) {
    // All subsets have ended and not one reached consensus!
    LOG_GENERAL(
        INFO,
        "[Subset " << subsetID
                   << "] Last remaining subset failed to reach consensus!");
    // Set overall state to ERROR
    m_state = ERROR;
  } else {
    LOG_GENERAL(
        INFO, "[Subset " << subsetID << "] Subset failed to reach consensus!");
  }
}

bool ConsensusLeader::ProcessMessageCommitCore(
    const bytes& commit, unsigned int offset, Action action,
    [[gnu::unused]] ConsensusMessageType returnmsgtype,
    [[gnu::unused]] State nextstate, const Peer& from) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutex);

  // Initial checks
  // ==============

  if (!CheckState(action)) {
    return false;
  }

  // Extract and check commit message body
  // =====================================

  uint16_t backupID = 0;

  CommitPoint commitPoint;
  CommitPointHash commitPointHash;

  if (!Messenger::GetConsensusCommit(
          commit, offset, m_consensusID, m_blockNumber, m_blockHash, backupID,
          commitPoint, commitPointHash, m_committee)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCommit failed");
    return false;
  }

  // Check the IP belongs to the backup with that backupID (check for valid
  // backupID range is already done in Messenger)
  if (m_committee.at(backupID).second.m_ipAddress != from.m_ipAddress) {
    LOG_CHECK_FAIL("Backup IP", from.GetPrintableIPAddress(),
                   m_committee.at(backupID).second.GetPrintableIPAddress());
    return false;
  }

  if (m_commitMap.at(backupID)) {
    LOG_GENERAL(WARNING, "Backup already sent commit");
    return false;
  }

  // Check the commit
  if (!commitPoint.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid commit");
    return false;
  }

  // Check the deserialized commit hash
  if (!commitPointHash.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid commit hash");
    return false;
  }

  // Check the value of the commit hash
  CommitPointHash commitPointHashExpected(commitPoint);
  if (!(commitPointHashExpected == commitPointHash)) {
    LOG_CHECK_FAIL("Commit hash", string(commitPointHash),
                   string(commitPointHashExpected));
    return false;
  }

  // Update internal state
  // =====================

  if (!CheckState(action)) {
    return false;
  }

  // 33-byte commit
  m_commitPoints.emplace_back(commitPoint);
  m_commitPointMap.at(backupID) = commitPoint;
  m_commitMap.at(backupID) = true;

  m_commitCounter++;

  if (m_commitCounter % 10 == 0) {
    LOG_GENERAL(INFO, "Received commits = " << m_commitCounter << " / "
                                            << m_numForConsensus);
  }

  // Redundant commits
  if (m_commitCounter > m_numForConsensus) {
    m_commitRedundantPointMap.at(backupID) = commitPoint;
    m_commitRedundantMap.at(backupID) = true;
    m_commitRedundantCounter++;
  }

  if (m_numOfSubsets > 1) {
    // notify the waiting thread to start with subset creations and subset
    // consensus.
    if (m_commitCounter == m_sufficientCommitsNumForSubsets) {
      lock_guard<mutex> g(m_mutexAnnounceSubsetConsensus);
      m_sufficientCommitsReceived = true;
      cv_scheduleSubsetConsensus.notify_all();
    }
  } else {
    if (m_commitCounter == m_numForConsensus) {
      LOG_GENERAL(INFO, "Sufficient commits");
      GenerateConsensusSubsets();
      if (!StartConsensusSubsets()) {
        LOG_GENERAL(WARNING, "StartConsensusSubsets failed");
      }
    }
  }
  return true;
}

bool ConsensusLeader::ProcessMessageCommit(const bytes& commit,
                                           unsigned int offset,
                                           const Peer& from) {
  return ProcessMessageCommitCore(commit, offset, PROCESS_COMMIT, CHALLENGE,
                                  CHALLENGE_DONE, from);
}

bool ConsensusLeader::ProcessMessageCommitFailure(const bytes& commitFailureMsg,
                                                  unsigned int offset,
                                                  const Peer& from) {
  LOG_MARKER();

  if (!CheckState(PROCESS_COMMITFAILURE)) {
    return false;
  }

  uint16_t backupID = 0;
  bytes errorMsg;

  if (!Messenger::GetConsensusCommitFailure(
          commitFailureMsg, offset, m_consensusID, m_blockNumber, m_blockHash,
          backupID, errorMsg, m_committee)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCommitFailure failed");
    return false;
  }

  if (m_commitFailureMap.find(backupID) != m_commitFailureMap.end()) {
    LOG_GENERAL(WARNING, "Backup already sent commit failure message");
    return false;
  }

  m_commitFailureCounter++;
  m_commitFailureMap[backupID] = errorMsg;
  m_nodeCommitFailureHandlerFunc(errorMsg, from);

  if (m_commitFailureCounter == m_numForConsensusFailure) {
    m_state = INITIAL;

    bytes consensusFailureMsg = {m_classByte, m_insByte, CONSENSUSFAILURE};

    if (!Messenger::SetConsensusConsensusFailure(
            consensusFailureMsg, MessageOffset::BODY + sizeof(uint8_t),
            m_consensusID, m_blockNumber, m_blockHash, m_myID,
            make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
      LOG_GENERAL(WARNING, "Messenger::SetConsensusConsensusFailure failed");
      return false;
    }

    deque<Peer> peerInfo;

    for (auto const& i : m_committee) {
      peerInfo.push_back(i.second);
    }

    P2PComm::GetInstance().SendMessage(peerInfo, consensusFailureMsg);
    auto main_func = [this]() mutable -> void {
      if (m_shardCommitFailureHandlerFunc != nullptr) {
        m_shardCommitFailureHandlerFunc(m_commitFailureMap);
      }
    };
    DetachedFunction(1, main_func);
  }

  return true;
}

bool ConsensusLeader::GenerateChallengeMessage(bytes& challenge,
                                               unsigned int offset) {
  LOG_MARKER();

  vector<ChallengeSubsetInfo> subsetInfo;

  for (unsigned int subsetID = 0; subsetID < m_consensusSubsets.size();
       subsetID++) {
    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    ChallengeSubsetInfo si;

    // Generate challenge object
    // =========================

    // Aggregate commits
    si.aggregatedCommit = AggregateCommits(subset.commitPoints);
    if (!si.aggregatedCommit.Initialized()) {
      LOG_GENERAL(WARNING,
                  "[Subset " << subsetID << "] AggregateCommits failed");
      return false;
    }

    // Aggregate keys
    si.aggregatedKey = AggregateKeys(subset.commitMap);

    // Generate the challenge
    subset.challenge =
        GetChallenge(m_messageToCosign, si.aggregatedCommit, si.aggregatedKey);

    si.challenge = subset.challenge;

    if (!subset.challenge.Initialized() || !si.challenge.Initialized()) {
      LOG_GENERAL(WARNING, "Challenge generation failed");
      return false;
    }

    subsetInfo.emplace_back(si);
  }

  // Assemble challenge message body
  // ===============================

  if (!Messenger::SetConsensusChallenge(
          challenge, offset, m_consensusID, m_blockNumber, m_blockHash, m_myID,
          subsetInfo,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusChallenge failed");
    return false;
  }

  return true;
}

bool ConsensusLeader::ProcessMessageResponseCore(
    const bytes& response, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate, const Peer& from) {
  LOG_MARKER();
  // Initial checks
  // ==============

  if (!CheckState(action)) {
    return false;
  }

  // Extract and check response message body
  // =======================================

  uint16_t backupID = 0;
  vector<ResponseSubsetInfo> subsetInfo;

  if (!Messenger::GetConsensusResponse(response, offset, m_consensusID,
                                       m_blockNumber, m_blockHash, backupID,
                                       subsetInfo, m_committee)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusResponse failed");
    return false;
  }

  // Check the IP belongs to the backup with that backupID (check for valid
  // backupID range is already done in Messenger)
  if (m_committee.at(backupID).second.m_ipAddress != from.m_ipAddress) {
    LOG_CHECK_FAIL("Backup IP", from.GetPrintableIPAddress(),
                   m_committee.at(backupID).second.GetPrintableIPAddress());
    return false;
  }

  // Check the subset size
  if (subsetInfo.size() > m_consensusSubsets.size()) {
    LOG_GENERAL(WARNING, "Response count " << subsetInfo.size() << " > "
                                           << m_consensusSubsets.size());
    return false;
  }

  for (unsigned int subsetID = 0; subsetID < subsetInfo.size(); subsetID++) {
    // Check subset state
    if (!CheckStateSubset(subsetID, action)) {
      continue;
    }

    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    // Check the backup id
    if (backupID >= subset.responseDataMap.size()) {
      LOG_GENERAL(WARNING, "[Subset "
                               << subsetID << "] Backup ID " << backupID
                               << " >= " << subset.responseDataMap.size());
      continue;
    }

    if (!subset.commitMap.at(backupID)) {
      LOG_GENERAL(WARNING, "[Subset " << subsetID << "] [Backup " << backupID
                                      << "] Didn't commit");
      continue;
    }

    if (subset.responseMap.at(backupID)) {
      LOG_GENERAL(WARNING, "[Subset " << subsetID << "] [Backup " << backupID
                                      << "] Already responded");
      continue;
    }

    if (!MultiSig::VerifyResponse(subsetInfo.at(subsetID).response,
                                  subset.challenge,
                                  GetCommitteeMember(backupID).first,
                                  subset.commitPointMap.at(backupID))) {
      LOG_GENERAL(WARNING, "[Subset " << subsetID << "] [Backup " << backupID
                                      << "] Invalid response");
      continue;
    }

    // Update internal state
    // =====================

    lock_guard<mutex> g(m_mutex);
    if (!CheckState(action)) {
      return false;
    }

    if (!CheckStateSubset(subsetID, action)) {
      return false;
    }

    // 32-byte response
    subset.responseData.emplace_back(subsetInfo.at(subsetID).response);
    subset.responseDataMap.at(backupID) = subsetInfo.at(subsetID).response;
    subset.responseMap.at(backupID) = true;
    subset.responseCounter++;

    if (subset.responseCounter % 10 == 0) {
      LOG_GENERAL(INFO, "[Subset " << subsetID << "] Received responses = "
                                   << subset.responseCounter << " / "
                                   << m_numForConsensus);
    }

    // Generate collective sig if sufficient responses have been obtained
    // ==================================================================

    if (subset.responseCounter == m_numForConsensus) {
      LOG_GENERAL(INFO, "[Subset " << subsetID << "] Sufficient responses");

      bytes collectivesig = {m_classByte, m_insByte,
                             static_cast<uint8_t>(returnmsgtype)};
      if (!GenerateCollectiveSigMessage(
              collectivesig, MessageOffset::BODY + sizeof(uint8_t), subsetID)) {
        LOG_GENERAL(WARNING, "GenerateCollectiveSigMessage failed");
        return false;
      }

      // Update internal state
      // =====================
      // Update subset's internal state
      SetStateSubset(subsetID, nextstate);
      m_state = nextstate;
      if (action == PROCESS_RESPONSE) {
        // First round: consensus over part of message (e.g., DS block header)
        // Second round: consensus over part of message + CS1 + B1
        subset.collectiveSig.Serialize(m_messageToCosign,
                                       m_messageToCosign.size());
        BitVector::SetBitVector(m_messageToCosign, m_messageToCosign.size(),
                                subset.responseMap);

        // Save the collective sig over the first round
        m_CS1 = subset.collectiveSig;
        m_B1 = subset.responseMap;

        // reset settings for second round of consensus
        m_commitMap.resize(m_committee.size());
        fill(m_commitMap.begin(), m_commitMap.end(), false);
        m_commitPointMap.resize(m_committee.size());
        m_commitPoints.clear();

        // Add the leader to the commits
        m_commitMap.at(m_myID) = true;
        m_commitPoints.emplace_back(*m_commitPoint);
        m_commitPointMap.at(m_myID) = *m_commitPoint;
        m_commitCounter = 1;

        m_commitFailureCounter = 0;
        m_commitFailureMap.clear();

        m_commitRedundantCounter = 0;
        fill(m_commitRedundantMap.begin(), m_commitRedundantMap.end(), false);

      } else {
        // Save the collective sig over the second round
        m_CS2 = subset.collectiveSig;
        m_B2 = subset.responseMap;
      }

      // Subset has finished consensus! Either Round 1 or Round 2
      SubsetEnded(subsetID);

      // Multicast to all nodes in the committee
      // =======================================

      deque<Peer> peerInfo;

      for (auto const& i : m_committee) {
        peerInfo.push_back(i.second);
      }

      if (BROADCAST_GOSSIP_MODE) {
        P2PComm::GetInstance().SpreadRumor(collectivesig);
      } else {
        P2PComm::GetInstance().SendMessage(peerInfo, collectivesig);
      }

      if ((m_state == COLLECTIVESIG_DONE) && (m_numOfSubsets > 1)) {
        // Start timer for accepting final commits
        // =================================
        auto func = [this]() -> void {
          std::unique_lock<std::mutex> cv_lk(m_mutexAnnounceSubsetConsensus);
          m_sufficientCommitsReceived = false;
          if (cv_scheduleSubsetConsensus.wait_for(
                  cv_lk, std::chrono::seconds(COMMIT_WINDOW_IN_SECONDS),
                  [&] { return m_sufficientCommitsReceived; })) {
            LOG_GENERAL(INFO, "Sufficient final commits within window")
          } else {
            LOG_GENERAL(INFO, "Timeout - Final Commit window closed");
          }
          if (m_commitCounter < m_numForConsensus) {
            LOG_GENERAL(WARNING,
                        "Insufficient final commits. Required "
                        "= " << m_numForConsensus
                             << " Actual = " << m_commitCounter);
            m_state = ERROR;
          } else {
            LOG_GENERAL(INFO, "Sufficient final commits. Required = "
                                  << m_numForConsensus
                                  << " Actual = " << m_commitCounter);
            lock_guard<mutex> g(m_mutex);
            GenerateConsensusSubsets();
            if (!StartConsensusSubsets()) {
              LOG_GENERAL(WARNING, "StartConsensusSubsets failed");
            }
          }
        };
        DetachedFunction(1, func);
      }

      break;
    }
  }

  return true;
}

bool ConsensusLeader::ProcessMessageResponse(const bytes& response,
                                             unsigned int offset,
                                             const Peer& from) {
  return ProcessMessageResponseCore(response, offset, PROCESS_RESPONSE,
                                    COLLECTIVESIG, COLLECTIVESIG_DONE, from);
}

bool ConsensusLeader::GenerateCollectiveSigMessage(bytes& collectivesig,
                                                   unsigned int offset,
                                                   uint16_t subsetID) {
  LOG_MARKER();

  // Generate collective signature object
  // ====================================

  ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

  // Aggregate responses
  Response aggregated_response = AggregateResponses(subset.responseData);
  if (!aggregated_response.Initialized()) {
    LOG_GENERAL(WARNING, "AggregateCommits failed");
    SetStateSubset(subsetID, ERROR);
    return false;
  }

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(subset.responseMap);

  // Generate the collective signature
  subset.collectiveSig = AggregateSign(subset.challenge, aggregated_response);

  // Verify the collective signature
  if (!MultiSig::GetInstance().MultiSigVerify(
          m_messageToCosign, subset.collectiveSig, aggregated_key)) {
    LOG_GENERAL(WARNING, "MultiSigVerify failed");
    SetStateSubset(subsetID, ERROR);
    return false;
  }

  // Assemble collective signature message body
  // ==========================================

  if (!Messenger::SetConsensusCollectiveSig(
          collectivesig, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_myID, subset.collectiveSig, subset.responseMap,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCollectiveSig failed.");
    return false;
  }

  // set the collective sig of overall state
  m_collectiveSig = subset.collectiveSig;

  return true;
}

bool ConsensusLeader::ProcessMessageFinalCommit(const bytes& finalcommit,
                                                unsigned int offset,
                                                const Peer& from) {
  return ProcessMessageCommitCore(finalcommit, offset, PROCESS_FINALCOMMIT,
                                  FINALCHALLENGE, FINALCHALLENGE_DONE, from);
}

bool ConsensusLeader::ProcessMessageFinalResponse(const bytes& finalresponse,
                                                  unsigned int offset,
                                                  const Peer& from) {
  return ProcessMessageResponseCore(finalresponse, offset,
                                    PROCESS_FINALRESPONSE, FINALCOLLECTIVESIG,
                                    DONE, from);
}

ConsensusLeader::ConsensusLeader(
    uint32_t consensus_id, uint64_t block_number, const bytes& block_hash,
    uint16_t node_id, const PrivKey& privkey, const DequeOfNode& committee,
    unsigned char class_byte, unsigned char ins_byte,
    NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
    ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc, bool isDS)
    : ConsensusCommon(consensus_id, block_number, block_hash, node_id, privkey,
                      committee, class_byte, ins_byte),
      m_DS(isDS),
      m_commitMap(committee.size(), false),
      m_commitPointMap(committee.size(), CommitPoint()),
      m_commitRedundantMap(committee.size(), false),
      m_commitRedundantPointMap(committee.size(), CommitPoint()) {
  LOG_MARKER();

  m_numOfSubsets =
      m_DS ? DS_NUM_CONSENSUS_SUBSETS : SHARD_NUM_CONSENSUS_SUBSETS;

  m_state = INITIAL;
  // m_numForConsensus = (floor(TOLERANCE_FRACTION * (pubkeys.size() - 1)) + 1);
  m_numForConsensus = ConsensusCommon::NumForConsensus(committee.size());
  m_numForConsensusFailure = committee.size() - m_numForConsensus;

  m_nodeCommitFailureHandlerFunc = nodeCommitFailureHandlerFunc;
  m_shardCommitFailureHandlerFunc = shardCommitFailureHandlerFunc;

  m_commitSecret.reset(new CommitSecret());
  m_commitPoint.reset(new CommitPoint(*m_commitSecret));

  // Add the leader to the commits
  m_commitMap.at(m_myID) = true;
  m_commitPoints.emplace_back(*m_commitPoint);
  m_commitPointMap.at(m_myID) = *m_commitPoint;
  m_commitCounter = 1;

  m_sufficientCommitsReceived = false;
  m_commitRedundantCounter = 0;
  m_commitFailureCounter = 0;
  m_numSubsetsRunning = 0;

  m_sufficientCommitsNumForSubsets =
      ceil((double)m_committee.size() * COMMIT_TOLERANCE_PERCENT / 100);

  LOG_GENERAL(INFO, "Consensus ID       = " << m_consensusID);
  LOG_GENERAL(INFO, "Leader/My ID       = " << m_myID);
  LOG_GENERAL(INFO, "Committee size     = " << committee.size());
  LOG_GENERAL(INFO, "Num for consensus  = " << m_numForConsensus);
  LOG_GENERAL(INFO, "Num for failure    = " << m_numForConsensusFailure);
  if (m_DS) {
    LOG_GENERAL(INFO,
                "Needed for subsets = " << m_sufficientCommitsNumForSubsets);
  }
}

ConsensusLeader::~ConsensusLeader() {}

bool ConsensusLeader::StartConsensus(
    AnnouncementGeneratorFunc announcementGeneratorFunc, bool useGossipProto) {
  LOG_MARKER();

  // Initial checks
  // ==============

  if (!CheckState(SEND_ANNOUNCEMENT)) {
    return false;
  }

  // Assemble announcement message body
  // ==================================
  bytes announcement_message = {m_classByte, m_insByte,
                                ConsensusMessageType::ANNOUNCE};

  if (!announcementGeneratorFunc(
          announcement_message, MessageOffset::BODY + sizeof(uint8_t),
          m_consensusID, m_blockNumber, m_blockHash, m_myID,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first),
          m_messageToCosign)) {
    LOG_GENERAL(WARNING, "Failed to generate announcement message");
    return false;
  }

  // Update internal state
  // =====================

  m_state = ANNOUNCE_DONE;
  m_commitRedundantCounter = 0;
  m_commitFailureCounter = 0;

  // Multicast to all nodes in the committee
  // =======================================

  if (useGossipProto) {
    P2PComm::GetInstance().SpreadRumor(announcement_message);
  } else {
    std::deque<Peer> peer;

    for (auto const& i : m_committee) {
      peer.push_back(i.second);
    }

    P2PComm::GetInstance().SendMessage(peer, announcement_message);
  }

  if (m_numOfSubsets > 1) {
    // Start timer for accepting commits
    // =================================
    auto func = [this]() -> void {
      std::unique_lock<std::mutex> cv_lk(m_mutexAnnounceSubsetConsensus);
      m_sufficientCommitsReceived = false;
      if (cv_scheduleSubsetConsensus.wait_for(
              cv_lk, std::chrono::seconds(COMMIT_WINDOW_IN_SECONDS),
              [&] { return m_sufficientCommitsReceived; })) {
        LOG_GENERAL(INFO, "Sufficient commits within window");
      } else {
        LOG_GENERAL(INFO, "Timeout - Commit window closed");
      }

      if (m_commitCounter < m_numForConsensus) {
        LOG_GENERAL(WARNING, "Insufficient commits. Required = "
                                 << m_numForConsensus
                                 << " Actual = " << m_commitCounter);
        m_state = ERROR;
      } else {
        LOG_GENERAL(INFO, "Sufficient commits. Required = " << m_numForConsensus
                                                            << " Actual = "
                                                            << m_commitCounter);
        lock_guard<mutex> g(m_mutex);
        GenerateConsensusSubsets();
        StartConsensusSubsets();
      }
    };
    DetachedFunction(1, func);
  }

  return true;
}

bool ConsensusLeader::ProcessMessage(const bytes& message, unsigned int offset,
                                     const Peer& from) {
  LOG_MARKER();

  // Incoming message format (from offset): [1-byte consensus message type]
  // [consensus message]

  bool result = false;

  switch (message.at(offset)) {
    case ConsensusMessageType::COMMIT:
      result = ProcessMessageCommit(message, offset + 1, from);
      break;
    case ConsensusMessageType::COMMITFAILURE:
      result = ProcessMessageCommitFailure(message, offset + 1, from);
      break;
    case ConsensusMessageType::RESPONSE:
      result = ProcessMessageResponse(message, offset + 1, from);
      break;
    case ConsensusMessageType::FINALCOMMIT:
      result = ProcessMessageFinalCommit(message, offset + 1, from);
      break;
    case ConsensusMessageType::FINALRESPONSE:
      result = ProcessMessageFinalResponse(message, offset + 1, from);
      break;
    default:
      LOG_GENERAL(WARNING,
                  "Unknown msg type " << (unsigned int)message.at(offset));
  }

  return result;
}

void ConsensusLeader::Audit() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutex);

  for (unsigned int subsetID = 0; subsetID < m_consensusSubsets.size();
       subsetID++) {
    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    LogResponsesStats(subsetID);

    if ((subset.state == CHALLENGE_DONE) ||
        (subset.state == FINALCHALLENGE_DONE)) {
      if (subset.commitMap.size() != m_committee.size()) {
        LOG_GENERAL(WARNING, "Wrong commit map size");
        continue;
      }
      if (subset.commitMap.size() != subset.responseMap.size()) {
        LOG_GENERAL(WARNING, "Wrong response map size");
        continue;
      }

      LOG_GENERAL(INFO, "[Subset " << subsetID << "] State = "
                                   << GetStateString(subset.state));
      LOG_GENERAL(INFO, "Missing responses:");

      for (unsigned int peerIndex = 0; peerIndex < m_committee.size();
           peerIndex++) {
        if (subset.commitMap.at(peerIndex) &&
            !subset.responseMap.at(peerIndex)) {
          LOG_GENERAL(INFO, "[" << PAD(peerIndex, 3, ' ') << "] "
                                << m_committee.at(peerIndex).second);
        }
      }
    }
  }
}

#define MAKE_LITERAL_PAIR(s) \
  { s, #s }

map<ConsensusLeader::Action, string> ConsensusLeader::ActionStrings = {
    MAKE_LITERAL_PAIR(SEND_ANNOUNCEMENT),
    MAKE_LITERAL_PAIR(PROCESS_COMMIT),
    MAKE_LITERAL_PAIR(PROCESS_RESPONSE),
    MAKE_LITERAL_PAIR(PROCESS_FINALCOMMIT),
    MAKE_LITERAL_PAIR(PROCESS_FINALRESPONSE),
    MAKE_LITERAL_PAIR(PROCESS_COMMITFAILURE)};

std::string ConsensusLeader::GetActionString(Action action) const {
  return (ActionStrings.find(action) == ActionStrings.end())
             ? "UNKNOWN"
             : ActionStrings.at(action);
}
