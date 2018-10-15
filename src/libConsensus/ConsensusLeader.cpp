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

bool ConsensusLeader::ProcessMessageCommitCore(
    const vector<unsigned char>& commit, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate) {
  LOG_MARKER();

  // Initial checks
  // ==============

  if (!CheckState(action)) {
    return false;
  }

  // Extract and check commit message body
  // =====================================

  uint16_t backupID = 0;

  CommitPoint commitPoint;

  if (!Messenger::GetConsensusCommit(commit, offset, m_consensusID,
                                     m_blockNumber, m_blockHash, backupID,
                                     commitPoint, m_committee)) {
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

  bool result = false;
  {
    // Update internal state
    // =====================
    lock_guard<mutex> g(m_mutex);

    if (!CheckState(action)) {
      return false;
    }

    // 33-byte commit
    if (m_commitCounter < m_numForConsensus) {
      m_commitPoints.emplace_back(commitPoint);
      m_commitPointMap.at(backupID) = commitPoint;
      m_commitMap.at(backupID) = true;
    }
    m_commitCounter++;

    if (m_commitCounter % 10 == 0) {
      LOG_GENERAL(INFO, "Received " << m_commitCounter << " out of "
                                    << m_numForConsensus << ".");
    }

    // Generate challenge if sufficient commits have been obtained
    // ===========================================================

    if (m_commitCounter == m_numForConsensus) {
      LOG_GENERAL(INFO,
                  "Sufficient " << m_numForConsensus << " commits obtained");

      vector<unsigned char> challenge = {
          m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype)};
      result = GenerateChallengeMessage(
          challenge, MessageOffset::BODY + sizeof(unsigned char));
      if (result) {
        // Update internal state
        // =====================

        m_state = nextstate;

        // Add the leader to the responses
        Response r(*m_commitSecret, m_challenge, m_myPrivKey);
        m_responseData.emplace_back(r);
        m_responseDataMap.at(m_myID) = r;
        m_responseMap.at(m_myID) = true;
        m_responseCounter = 1;

        // Multicast to all nodes who send validated commits
        // =================================================

        vector<Peer> commit_peers;
        deque<pair<PubKey, Peer>>::const_iterator j = m_committee.begin();

        for (unsigned int i = 0; i < m_commitMap.size(); i++, j++) {
          if ((m_commitMap.at(i)) && (i != m_myID)) {
            commit_peers.emplace_back(j->second);
          }
        }
        if (BROADCAST_GOSSIP_MODE) {
          P2PComm::GetInstance().SpreadRumor(challenge);
        } else {
          P2PComm::GetInstance().SendMessage(commit_peers, challenge);
        }
      }
    }

    // Redundant commits
    if (m_commitCounter > m_numForConsensus) {
      m_commitRedundantPointMap.at(backupID) = commitPoint;
      m_commitRedundantMap.at(backupID) = true;
      m_commitRedundantCounter++;
    }
  }

  return result;
}

bool ConsensusLeader::ProcessMessageCommit(const vector<unsigned char>& commit,
                                           unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCommitCore(commit, offset, PROCESS_COMMIT, CHALLENGE,
                                  CHALLENGE_DONE);
}

bool ConsensusLeader::ProcessMessageCommitFailure(
    const vector<unsigned char>& commitFailureMsg, unsigned int offset,
    const Peer& from) {
  LOG_MARKER();

  if (!CheckState(PROCESS_COMMITFAILURE)) {
    return false;
  }

  uint16_t backupID = 0;
  vector<unsigned char> errorMsg;

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

    vector<unsigned char> consensusFailureMsg = {m_classByte, m_insByte,
                                                 CONSENSUSFAILURE};
    deque<Peer> peerInfo;

    for (auto const& i : m_committee) {
      peerInfo.push_back(i.second);
    }

    P2PComm::GetInstance().SendMessage(peerInfo, consensusFailureMsg);
    auto main_func = [this]() mutable -> void {
      m_shardCommitFailureHandlerFunc(m_commitFailureMap);
    };
    DetachedFunction(1, main_func);
  }

  return true;
}

bool ConsensusLeader::GenerateChallengeMessage(vector<unsigned char>& challenge,
                                               unsigned int offset) {
  LOG_MARKER();

  // Generate challenge object
  // =========================

  // Aggregate commits
  CommitPoint aggregated_commit = AggregateCommits(m_commitPoints);
  if (!aggregated_commit.Initialized()) {
    LOG_GENERAL(WARNING, "AggregateCommits failed");
    m_state = ERROR;
    return false;
  }

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(m_commitMap);
  if (!aggregated_key.Initialized()) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    m_state = ERROR;
    return false;
  }

  // Generate the challenge
  m_challenge =
      GetChallenge(m_messageToCosign, aggregated_commit, aggregated_key);
  if (!m_challenge.Initialized()) {
    LOG_GENERAL(WARNING, "Challenge generation failed");
    m_state = ERROR;
    return false;
  }

  // Assemble challenge message body
  // ===============================

  if (!Messenger::SetConsensusChallenge(
          challenge, offset, m_consensusID, m_blockNumber, m_blockHash, m_myID,
          aggregated_commit, aggregated_key, m_challenge,
          make_pair(m_myPrivKey, m_committee.at(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusChallenge failed.");
    return false;
  }

  return true;
}

bool ConsensusLeader::ProcessMessageResponseCore(
    const vector<unsigned char>& response, unsigned int offset, Action action,
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
  Response r;

  if (!Messenger::GetConsensusResponse(response, offset, m_consensusID,
                                       m_blockNumber, m_blockHash, backupID, r,
                                       m_committee)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusResponse failed.");
    return false;
  }

  if (!m_commitMap.at(backupID)) {
    LOG_GENERAL(WARNING, "Backup has not participated in the commit phase");
    return false;
  }

  if (m_responseMap.at(backupID)) {
    LOG_GENERAL(WARNING, "Backup has already sent validated response");
    return false;
  }

  if (!MultiSig::VerifyResponse(r, m_challenge, m_committee.at(backupID).first,
                                m_commitPointMap.at(backupID))) {
    LOG_GENERAL(WARNING, "Invalid response for this backup");
    return false;
  }

  // Update internal state
  // =====================

  lock_guard<mutex> g(m_mutex);

  if (!CheckState(action)) {
    return false;
  }

  // 32-byte response
  m_responseData.emplace_back(r);
  m_responseDataMap.at(backupID) = r;
  m_responseMap.at(backupID) = true;
  m_responseCounter++;

  // Generate collective sig if sufficient responses have been obtained
  // ==================================================================

  bool result = true;

  if (m_responseCounter == m_numForConsensus) {
    LOG_GENERAL(INFO, "Sufficient responses obtained");

    vector<unsigned char> collectivesig = {
        m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype)};
    result = GenerateCollectiveSigMessage(
        collectivesig, MessageOffset::BODY + sizeof(unsigned char));

    if (result) {
      // Update internal state
      // =====================

      m_state = nextstate;

      if (action == PROCESS_RESPONSE) {
        // First round: consensus over part of message (e.g., DS block header)
        // Second round: consensus over part of message + CS1 + B1
        m_collectiveSig.Serialize(m_messageToCosign, m_messageToCosign.size());
        BitVector::SetBitVector(m_messageToCosign, m_messageToCosign.size(),
                                m_responseMap);

        // Save the collective sig over the first round
        m_CS1 = m_collectiveSig;
        m_B1 = m_responseMap;

        m_commitPoints.clear();
        fill(m_commitMap.begin(), m_commitMap.end(), false);

        // Add the leader to the commits
        m_commitMap.at(m_myID) = true;
        m_commitPoints.emplace_back(*m_commitPoint);
        m_commitPointMap.at(m_myID) = *m_commitPoint;
        m_commitCounter = 1;

        m_commitFailureCounter = 0;
        m_commitFailureMap.clear();

        m_commitRedundantCounter = 0;
        fill(m_commitRedundantMap.begin(), m_commitRedundantMap.end(), false);

        m_responseCounter = 0;
        m_responseData.clear();
        fill(m_responseMap.begin(), m_responseMap.end(), false);
      } else {
        // Save the collective sig over the second round
        m_CS2 = m_collectiveSig;
        m_B2 = m_responseMap;
      }

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
    }
  }

  return result;
}

bool ConsensusLeader::ProcessMessageResponse(
    const vector<unsigned char>& response, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageResponseCore(response, offset, PROCESS_RESPONSE,
                                    COLLECTIVESIG, COLLECTIVESIG_DONE);
}

bool ConsensusLeader::GenerateCollectiveSigMessage(
    vector<unsigned char>& collectivesig, unsigned int offset) {
  LOG_MARKER();

  // Generate collective signature object
  // ====================================

  // Aggregate responses
  Response aggregated_response = AggregateResponses(m_responseData);
  if (!aggregated_response.Initialized()) {
    LOG_GENERAL(WARNING, "AggregateCommits failed");
    m_state = ERROR;
    return false;
  }

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(m_responseMap);
  if (!aggregated_key.Initialized()) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    m_state = ERROR;
    return false;
  }

  // Generate the collective signature
  m_collectiveSig = AggregateSign(m_challenge, aggregated_response);
  if (!m_collectiveSig.Initialized()) {
    LOG_GENERAL(WARNING, "Collective sig generation failed");
    m_state = ERROR;
    return false;
  }

  // Verify the collective signature
  if (!Schnorr::GetInstance().Verify(m_messageToCosign, m_collectiveSig,
                                     aggregated_key)) {
    LOG_GENERAL(WARNING, "Collective sig verification failed");
    m_state = ERROR;

    LOG_GENERAL(INFO, "num of pub keys: " << m_committee.size() << " "
                                          << "num of peer_info keys: "
                                          << m_committee.size());
    return false;
  }

  // Assemble collective signature message body
  // ==========================================

  if (!Messenger::SetConsensusCollectiveSig(
          collectivesig, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_myID, m_collectiveSig, m_responseMap,
          make_pair(m_myPrivKey, m_committee.at(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCollectiveSig failed.");
    return false;
  }

  return true;
}

bool ConsensusLeader::ProcessMessageFinalCommit(
    const vector<unsigned char>& finalcommit, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCommitCore(finalcommit, offset, PROCESS_FINALCOMMIT,
                                  FINALCHALLENGE, FINALCHALLENGE_DONE);
}

bool ConsensusLeader::ProcessMessageFinalResponse(
    const vector<unsigned char>& finalresponse, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageResponseCore(
      finalresponse, offset, PROCESS_FINALRESPONSE, FINALCOLLECTIVESIG, DONE);
}

ConsensusLeader::ConsensusLeader(
    uint32_t consensus_id, uint64_t block_number,
    const vector<unsigned char>& block_hash, uint16_t node_id,
    const PrivKey& privkey, const deque<pair<PubKey, Peer>>& committee,
    unsigned char class_byte, unsigned char ins_byte,
    NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
    ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc)
    : ConsensusCommon(consensus_id, block_number, block_hash, node_id, privkey,
                      committee, class_byte, ins_byte),
      m_commitMap(committee.size(), false),
      m_commitPointMap(committee.size(), CommitPoint()),
      m_commitRedundantMap(committee.size(), false),
      m_commitRedundantPointMap(committee.size(), CommitPoint()),
      m_responseDataMap(committee.size(), Response()) {
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
  vector<unsigned char> announcement_message = {m_classByte, m_insByte,
                                                ConsensusMessageType::ANNOUNCE};

  if (!announcementGeneratorFunc(
          announcement_message, MessageOffset::BODY + sizeof(unsigned char),
          m_consensusID, m_blockNumber, m_blockHash, m_myID,
          make_pair(m_myPrivKey, m_committee.at(m_myID).first),
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

  return true;
}

bool ConsensusLeader::ProcessMessage(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from) {
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
