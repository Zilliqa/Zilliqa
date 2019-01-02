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
    LOG_GENERAL(WARNING, "Action " << GetActionString(action)
                                   << " not allowed in state "
                                   << GetStateString());
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
    LOG_GENERAL(WARNING,
                "SubsetID: " << subsetID << ", Action "
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
  // Generate NUM_CONSENSUS_SUBSETS lists (= subsets of peersWhoCommitted)
  // If we have exactly the minimum num required for consensus, no point making
  // more than 1 subset

  const unsigned int numSubsets = (peersWhoCommitted.size() < m_numForConsensus)
                                      ? 1
                                      : NUM_CONSENSUS_SUBSETS;
  LOG_GENERAL(INFO, "PeerCommited:"
                        << peersWhoCommitted.size() + 1 << " m_numForConsensus:"
                        << m_numForConsensus << " numSubsets:" << numSubsets);

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

    for (unsigned int j = 0; j < m_numForConsensus - 1; j++) {
      unsigned int index = peersWhoCommitted.at(j);
      subset.commitPointMap.at(index) = m_commitPointMap.at(index);
      subset.commitPoints.emplace_back(m_commitPointMap.at(index));
      subset.commitMap.at(index) = true;
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
  LOG_GENERAL(INFO, "Generated " << numSubsets << " subsets of "
                                 << m_numForConsensus
                                 << " backups each for this consensus");
}

void ConsensusLeader::StartConsensusSubsets() {
  LOG_MARKER();

  ConsensusMessageType type;
  // Update overall internal state
  if (m_state == ANNOUNCE_DONE) {
    m_state = CHALLENGE_DONE;
    type = ConsensusMessageType::CHALLENGE;
  } else if (m_state == COLLECTIVESIG_DONE) {
    m_state = FINALCHALLENGE_DONE;
    type = ConsensusMessageType::FINALCHALLENGE;
  }

  m_numSubsetsRunning = m_consensusSubsets.size();
  for (unsigned int index = 0; index < m_consensusSubsets.size(); index++) {
    // If overall state has somehow transitioned from CHALLENGE_DONE or
    // FINALCHALLENGE_DONE then it means consensus has ended and there's no
    // point in starting another subset
    if (m_state != CHALLENGE_DONE && m_state != FINALCHALLENGE_DONE) {
      break;
    }
    ConsensusSubset& subset = m_consensusSubsets.at(index);
    bytes challenge = {m_classByte, m_insByte, static_cast<uint8_t>(type)};
    bool result = GenerateChallengeMessage(
        challenge, MessageOffset::BODY + sizeof(uint8_t), index);
    if (result) {
      // Update subset's internal state
      SetStateSubset(index, m_state);

      // Add the leader to the responses
      Response r(*m_commitSecret, subset.challenge, m_myPrivKey);
      subset.responseData.emplace_back(r);
      subset.responseDataMap.at(m_myID) = r;
      subset.responseMap.at(m_myID) = true;
      subset.responseCounter = 1;

      if (BROADCAST_GOSSIP_MODE) {
        // Gossip challenge within my all peers
        P2PComm::GetInstance().SpreadRumor(challenge);
      } else {
        // Multicast challenge to all nodes who send validated commits
        vector<Peer> commit_peers;
        deque<pair<PubKey, Peer>>::const_iterator j = m_committee.begin();

        for (unsigned int i = 0; i < subset.commitMap.size(); i++, j++) {
          if ((subset.commitMap.at(i)) && (i != m_myID)) {
            commit_peers.emplace_back(j->second);
          }
        }
        P2PComm::GetInstance().SendMessage(commit_peers, challenge);
      }
    } else {
      SetStateSubset(index, ERROR);
      SubsetEnded(index);
    }
  }
}
void ConsensusLeader::SubsetEnded(uint16_t subsetID) {
  LOG_MARKER();
  ConsensusSubset& subset = m_consensusSubsets.at(subsetID);
  if (subset.state == COLLECTIVESIG_DONE || subset.state == DONE) {
    // We've achieved consensus!
    LOG_GENERAL(INFO,
                "[Subset " << subsetID << "] Subset has finished consensus!");
    // Reset all other subsets to INITIAL so they reject any further messages
    // from their backups
    for (unsigned int i = 0; i < m_consensusSubsets.size(); i++) {
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
    LOG_GENERAL(INFO, "[Subset " << subsetID
                                 << "] Subset has failed to reach consensus!");
  }
}

bool ConsensusLeader::ProcessMessageCommitCore(
    const bytes& commit, unsigned int offset, Action action,
    [[gnu::unused]] ConsensusMessageType returnmsgtype,
    [[gnu::unused]] State nextstate) {
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
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCommit failed.");
    return false;
  }

  if (m_commitMap.at(backupID)) {
    LOG_GENERAL(WARNING, "Backup has already sent validated commit");
    return false;
  }

  // Check the commit
  if (!commitPoint.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid commit received");
    return false;
  }

  // Check the deserialized commit hash
  if (!commitPointHash.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid commit hash received");
    return false;
  }

  // Check the value of the commit hash
  CommitPointHash commitPointHashExpected(commitPoint);
  if (!(commitPointHashExpected == commitPointHash)) {
    LOG_GENERAL(WARNING, "Commit hash check failed. Deserialized = "
                             << string(commitPointHash) << " Expected = "
                             << string(commitPointHashExpected));
    return false;
  }

  bool result = false;

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
    LOG_GENERAL(INFO, "Received " << m_commitCounter << " out of "
                                  << m_numForConsensus << ".");
  }

  // Redundant commits
  if (m_commitCounter > m_numForConsensus) {
    m_commitRedundantPointMap.at(backupID) = commitPoint;
    m_commitRedundantMap.at(backupID) = true;
    m_commitRedundantCounter++;
  }

  if (NUM_CONSENSUS_SUBSETS > 1) {
    // notify the waiting thread to start with subset creations and subset
    // consensus.
    if (m_commitCounter == m_committee.size()) {
      lock_guard<mutex> g(m_mutexAnnounceSubsetConsensus);
      m_allCommitsReceived = true;
      cv_scheduleSubsetConsensus.notify_all();
    }
  } else {
    if (m_commitCounter == m_numForConsensus) {
      LOG_GENERAL(INFO, "Sufficient commits obtained. Required/Actual = "
                            << m_commitCounter);
      GenerateConsensusSubsets();
      StartConsensusSubsets();
    }
  }

  return result;
}

bool ConsensusLeader::ProcessMessageCommit(const bytes& commit,
                                           unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCommitCore(commit, offset, PROCESS_COMMIT, CHALLENGE,
                                  CHALLENGE_DONE);
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
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCommitFailure failed.");
    return false;
  }

  if (m_commitFailureMap.find(backupID) != m_commitFailureMap.end()) {
    LOG_GENERAL(WARNING, "Backup has already sent commit failure message");
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
      LOG_GENERAL(WARNING, "Messenger::SetConsensusConsensusFailure failed.");
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
                                               unsigned int offset,
                                               uint16_t subsetID) {
  LOG_MARKER();

  // Generate challenge object
  // =========================

  ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

  // Aggregate commits
  CommitPoint aggregated_commit = AggregateCommits(subset.commitPoints);
  if (!aggregated_commit.Initialized()) {
    LOG_GENERAL(WARNING, "[Subset " << subsetID << "] AggregateCommits failed");
    return false;
  }

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(subset.commitMap);
  if (!aggregated_key.Initialized()) {
    LOG_GENERAL(WARNING,
                "[Subset " << subsetID << "] Aggregated key generation failed");
    return false;
  }

  // Generate the challenge
  subset.challenge =
      GetChallenge(m_messageToCosign, aggregated_commit, aggregated_key);

  if (!subset.challenge.Initialized()) {
    LOG_GENERAL(WARNING, "Challenge generation failed");
    return false;
  }

  // Assemble challenge message body
  // ===============================

  if (!Messenger::SetConsensusChallenge(
          challenge, offset, m_consensusID, m_blockNumber, subsetID,
          m_blockHash, m_myID, aggregated_commit, aggregated_key,
          subset.challenge,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusChallenge failed.");
    return false;
  }

  return true;
}

bool ConsensusLeader::ProcessMessageResponseCore(
    const bytes& response, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate) {
  LOG_MARKER();
  // Initial checks
  // ==============

  if (!CheckState(action)) {
    return false;
  }

  // Extract and check response message body
  // =======================================

  uint16_t backupID = 0;
  uint16_t subsetID = 0;
  Response r;

  if (!Messenger::GetConsensusResponse(response, offset, m_consensusID,
                                       m_blockNumber, m_blockHash, backupID,
                                       subsetID, r, m_committee)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusResponse failed.");
    return false;
  }

  // Check the subset id
  if (subsetID >= m_consensusSubsets.size()) {
    LOG_GENERAL(WARNING, "Error: Subset ID (" << subsetID
                                              << ") >= NUM_CONSENSUS_SUBSETS: "
                                              << NUM_CONSENSUS_SUBSETS);
    return false;
  }

  // Check subset state
  if (!CheckStateSubset(subsetID, action)) {
    return false;
  }

  ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

  // Check the backup id
  if (backupID >= subset.responseDataMap.size()) {
    LOG_GENERAL(WARNING, "[Subset " << subsetID << "] [Backup " << backupID
                                    << "] Backup ID beyond backup count");
    return false;
  }
  if (!subset.commitMap.at(backupID)) {
    LOG_GENERAL(
        WARNING, "[Subset "
                     << subsetID << "] [Backup " << backupID
                     << "] Backup has not participated in the commit phase");
    return false;
  }

  if (subset.responseMap.at(backupID)) {
    LOG_GENERAL(WARNING, "[Subset "
                             << subsetID << "] [Backup " << backupID
                             << "] Backup has already sent validated response");
    return false;
  }

  if (!MultiSig::VerifyResponse(r, subset.challenge,
                                GetCommitteeMember(backupID).first,
                                subset.commitPointMap.at(backupID))) {
    LOG_GENERAL(WARNING, "Invalid response for this backup");
    return false;
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
  subset.responseData.emplace_back(r);
  subset.responseDataMap.at(backupID) = r;
  subset.responseMap.at(backupID) = true;
  subset.responseCounter++;

  if (subset.responseCounter % 10 == 0) {
    LOG_GENERAL(INFO, "[Subset " << subsetID << "] Received "
                                 << subset.responseCounter << " out of "
                                 << m_numForConsensus << ".");
  }

  // Generate collective sig if sufficient responses have been obtained
  // ==================================================================

  bool result = true;

  if (subset.responseCounter == m_numForConsensus) {
    LOG_GENERAL(INFO, "Sufficient responses obtained");

    bytes collectivesig = {m_classByte, m_insByte,
                           static_cast<uint8_t>(returnmsgtype)};
    result = GenerateCollectiveSigMessage(
        collectivesig, MessageOffset::BODY + sizeof(uint8_t), subsetID);

    if (result) {
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

      // FIXME: quick fix: 0106'08' comes to the backup ealier than 0106'04'
      // if (action == FINALCOMMIT)
      // {
      //     this_thread::sleep_for(chrono::milliseconds(1000));
      // }
      // this_thread::sleep_for(chrono::seconds(CONSENSUS_COSIG_WINDOW));

      deque<Peer> peerInfo;

      for (auto const& i : m_committee) {
        peerInfo.push_back(i.second);
      }

      if (BROADCAST_GOSSIP_MODE) {
        P2PComm::GetInstance().SpreadRumor(collectivesig);
      } else {
        P2PComm::GetInstance().SendMessage(peerInfo, collectivesig);
      }

      if ((m_state == COLLECTIVESIG_DONE) && (NUM_CONSENSUS_SUBSETS > 1)) {
        // Start timer for accepting final commits
        // =================================
        auto func = [this]() -> void {
          std::unique_lock<std::mutex> cv_lk(m_mutexAnnounceSubsetConsensus);
          m_allCommitsReceived = false;
          if (cv_scheduleSubsetConsensus.wait_for(
                  cv_lk, std::chrono::seconds(COMMIT_WINDOW_IN_SECONDS),
                  [&] { return m_allCommitsReceived; })) {
            LOG_GENERAL(
                INFO, "Received all final commits within the Commit window. !!")
          } else {
            LOG_GENERAL(INFO,
                        "Timeout - Final Commit window closed. Will process "
                        "commits received !!");
          }
          if (m_commitCounter < m_numForConsensus) {
            LOG_GENERAL(
                WARNING,
                "Insufficient final commits obtained after timeout. Required "
                "= " << m_numForConsensus
                     << " Actual = " << m_commitCounter);
            m_state = ERROR;
          } else {
            LOG_GENERAL(
                INFO,
                "Sufficient final commits obtained after timeout. Required = "
                    << m_numForConsensus << " Actual = " << m_commitCounter);
            lock_guard<mutex> g(m_mutex);
            GenerateConsensusSubsets();
            StartConsensusSubsets();
          }
        };
        DetachedFunction(1, func);
      }
    }
  }

  return result;
}

bool ConsensusLeader::ProcessMessageResponse(const bytes& response,
                                             unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageResponseCore(response, offset, PROCESS_RESPONSE,
                                    COLLECTIVESIG, COLLECTIVESIG_DONE);
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
  if (!aggregated_key.Initialized()) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    SetStateSubset(subsetID, ERROR);
    return false;
  }

  // Generate the collective signature
  subset.collectiveSig = AggregateSign(subset.challenge, aggregated_response);
  if (!subset.collectiveSig.Initialized()) {
    LOG_GENERAL(WARNING, "Collective sig generation failed");
    SetStateSubset(subsetID, ERROR);
    return false;
  }

  // Verify the collective signature
  if (!MultiSig::GetInstance().MultiSigVerify(
          m_messageToCosign, subset.collectiveSig, aggregated_key)) {
    LOG_GENERAL(WARNING, "Collective sig verification failed");
    SetStateSubset(subsetID, ERROR);

    LOG_GENERAL(INFO, "num of pub keys: " << m_committee.size() << " "
                                          << "num of peer_info keys: "
                                          << m_committee.size());
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
                                                unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCommitCore(finalcommit, offset, PROCESS_FINALCOMMIT,
                                  FINALCHALLENGE, FINALCHALLENGE_DONE);
}

bool ConsensusLeader::ProcessMessageFinalResponse(const bytes& finalresponse,
                                                  unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageResponseCore(
      finalresponse, offset, PROCESS_FINALRESPONSE, FINALCOLLECTIVESIG, DONE);
}

ConsensusLeader::ConsensusLeader(
    uint32_t consensus_id, uint64_t block_number, const bytes& block_hash,
    uint16_t node_id, const PrivKey& privkey,
    const deque<pair<PubKey, Peer>>& committee, unsigned char class_byte,
    unsigned char ins_byte,
    NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
    ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc)
    : ConsensusCommon(consensus_id, block_number, block_hash, node_id, privkey,
                      committee, class_byte, ins_byte),
      m_commitMap(committee.size(), false),
      m_commitPointMap(committee.size(), CommitPoint()),
      m_commitRedundantMap(committee.size(), false),
      m_commitRedundantPointMap(committee.size(), CommitPoint()) {
  LOG_MARKER();

  m_state = INITIAL;
  // m_numForConsensus = (floor(TOLERANCE_FRACTION * (pubkeys.size() - 1)) + 1);
  m_numForConsensus = ConsensusCommon::NumForConsensus(committee.size());
  m_numForConsensusFailure = committee.size() - m_numForConsensus;
  LOG_GENERAL(INFO, "TOLERANCE_FRACTION "
                        << TOLERANCE_FRACTION << " pubkeys.size() "
                        << committee.size() << " m_numForConsensus "
                        << m_numForConsensus << " m_numForConsensusFailure "
                        << m_numForConsensusFailure);

  m_nodeCommitFailureHandlerFunc = nodeCommitFailureHandlerFunc;
  m_shardCommitFailureHandlerFunc = shardCommitFailureHandlerFunc;

  m_commitSecret.reset(new CommitSecret());
  m_commitPoint.reset(new CommitPoint(*m_commitSecret));

  // Add the leader to the commits
  m_commitMap.at(m_myID) = true;
  m_commitPoints.emplace_back(*m_commitPoint);
  m_commitPointMap.at(m_myID) = *m_commitPoint;
  m_commitCounter = 1;

  m_allCommitsReceived = false;
  m_commitRedundantCounter = 0;
  m_commitFailureCounter = 0;
  m_numSubsetsRunning = 0;
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
    LOG_GENERAL(WARNING, "Failed to generate announcement message.");
    return false;
  }

  LOG_GENERAL(INFO, "Consensus id is " << m_consensusID
                                       << " Consensus leader id is " << m_myID);

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

  if (NUM_CONSENSUS_SUBSETS > 1) {
    // Start timer for accepting commits
    // =================================
    auto func = [this]() -> void {
      std::unique_lock<std::mutex> cv_lk(m_mutexAnnounceSubsetConsensus);
      m_allCommitsReceived = false;
      if (cv_scheduleSubsetConsensus.wait_for(
              cv_lk, std::chrono::seconds(COMMIT_WINDOW_IN_SECONDS),
              [&] { return m_allCommitsReceived; })) {
        LOG_GENERAL(INFO, "Received all commits within the Commit window. !!");
      } else {
        LOG_GENERAL(
            INFO,
            "Timeout - Commit window closed. Will process commits received !!");
      }

      if (m_commitCounter < m_numForConsensus) {
        LOG_GENERAL(WARNING,
                    "Insufficient commits obtained after timeout. Required = "
                        << m_numForConsensus
                        << " Actual = " << m_commitCounter);
        m_state = ERROR;
      } else {
        LOG_GENERAL(
            INFO, "Sufficient commits obtained after timeout. Required = "
                      << m_numForConsensus << " Actual = " << m_commitCounter);
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
      result = ProcessMessageCommit(message, offset + 1);
      break;
    case ConsensusMessageType::COMMITFAILURE:
      result = ProcessMessageCommitFailure(message, offset + 1, from);
      break;
    case ConsensusMessageType::RESPONSE:
      result = ProcessMessageResponse(message, offset + 1);
      break;
    case ConsensusMessageType::FINALCOMMIT:
      result = ProcessMessageFinalCommit(message, offset + 1);
      break;
    case ConsensusMessageType::FINALRESPONSE:
      result = ProcessMessageFinalResponse(message, offset + 1);
      break;
    default:
      LOG_GENERAL(WARNING, "Unknown consensus message received. No: "
                               << (unsigned int)message.at(offset));
  }

  return result;
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
             ? "Unknown"
             : ActionStrings.at(action);
}
