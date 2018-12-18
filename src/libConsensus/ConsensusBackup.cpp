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

#include "ConsensusBackup.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;

bool ConsensusBackup::CheckState(Action action) {
  static const std::multimap<ConsensusCommon::State, Action> ACTIONS_FOR_STATE =
      {{INITIAL, PROCESS_ANNOUNCE},
       {COMMIT_DONE, PROCESS_CHALLENGE},
       {COMMIT_DONE, PROCESS_COLLECTIVESIG},
       {COMMIT_DONE,
        PROCESS_FINALCOLLECTIVESIG},  // TODO: check this logic again. Issue #43
                                      // Node cannot proceed if
                                      // finalcollectivesig arrived earlier (and
                                      // get ignored by the node)
       {RESPONSE_DONE, PROCESS_CHALLENGE},
       {RESPONSE_DONE, PROCESS_COLLECTIVESIG},
       {RESPONSE_DONE,
        PROCESS_FINALCOLLECTIVESIG},  // TODO: check this logic again. Issue #43
                                      // Node cannot proceed if
                                      // finalcollectivesig arrived earlier (and
                                      // get ignored by the node)
       {FINALCOMMIT_DONE, PROCESS_FINALCHALLENGE},
       {FINALCOMMIT_DONE, PROCESS_FINALCOLLECTIVESIG},
       {FINALRESPONSE_DONE, PROCESS_FINALCHALLENGE},
       {FINALRESPONSE_DONE, PROCESS_FINALCOLLECTIVESIG}};

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

bool ConsensusBackup::ProcessMessageAnnounce(
    const vector<unsigned char>& announcement, unsigned int offset) {
  LOG_MARKER();

  // Initial checks
  // ==============

  if (!CheckState(PROCESS_ANNOUNCE)) {
    return false;
  }

  // Extract and check announce message body
  // =======================================

  std::vector<unsigned char> errorMsg;
  if (!m_msgContentValidator(announcement, offset, errorMsg, m_consensusID,
                             m_blockNumber, m_blockHash, m_leaderID,
                             GetCommitteeMember(m_leaderID).first,
                             m_messageToCosign)) {
    LOG_GENERAL(WARNING, "Message validation failed");

    if (!errorMsg.empty()) {
      vector<unsigned char> commitFailureMsg = {
          m_classByte, m_insByte,
          static_cast<unsigned char>(ConsensusMessageType::COMMITFAILURE)};

      bool result = GenerateCommitFailureMessage(
          commitFailureMsg, MessageOffset::BODY + sizeof(unsigned char),
          errorMsg);

      if (result) {
        // Update internal state
        // =====================
        m_state = ERROR;

        // Unicast to the leader
        // =====================
        P2PComm::GetInstance().SendMessage(
            GetCommitteeMember(m_leaderID).second, commitFailureMsg);

        return true;
      }
    }

    LOG_GENERAL(WARNING,
                "Announcement content validation failed - dropping message but "
                "keeping state");
    return false;
  }

  // Generate commit
  // ===============

  vector<unsigned char> commit = {
      m_classByte, m_insByte,
      static_cast<unsigned char>(ConsensusMessageType::COMMIT)};

  bool result = GenerateCommitMessage(
      commit, MessageOffset::BODY + sizeof(unsigned char));
  if (result) {
    // Update internal state
    // =====================
    m_state = COMMIT_DONE;

    // Unicast to the leader
    // =====================
    P2PComm::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                       commit);
  }
  return result;
}

bool ConsensusBackup::ProcessMessageConsensusFailure(
    const vector<unsigned char>& announcement, unsigned int offset) {
  LOG_MARKER();

  if (!Messenger::GetConsensusConsensusFailure(
          announcement, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_leaderID, GetCommitteeMember(m_leaderID).first)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusConsensusFailure failed.");
    return false;
  }

  m_state = INITIAL;

  return true;
}

bool ConsensusBackup::GenerateCommitFailureMessage(
    vector<unsigned char>& commitFailure, unsigned int offset,
    const vector<unsigned char>& errorMsg) {
  LOG_MARKER();

  if (!Messenger::SetConsensusCommitFailure(
          commitFailure, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_myID, errorMsg,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCommitFailure failed.");
    return false;
  }

  return true;
}

bool ConsensusBackup::GenerateCommitMessage(vector<unsigned char>& commit,
                                            unsigned int offset) {
  LOG_MARKER();

  // Generate new commit
  // ===================
  m_commitSecret.reset(new CommitSecret());
  m_commitPoint.reset(new CommitPoint(*m_commitSecret));

  // Assemble commit message body
  // ============================

  if (!Messenger::SetConsensusCommit(
          commit, offset, m_consensusID, m_blockNumber, m_blockHash, m_myID,
          *m_commitPoint,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCommit failed.");
    return false;
  }

  return true;
}

bool ConsensusBackup::ProcessMessageChallengeCore(
    const vector<unsigned char>& challenge, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate) {
  LOG_MARKER();

  // Initial checks
  // ==============

  if (!CheckState(action)) {
    return false;
  }

  // Extract and check challenge message body
  // ========================================

  CommitPoint aggregated_commit;
  PubKey aggregated_key;
  uint16_t subsetID = 0;

  if (!Messenger::GetConsensusChallenge(
          challenge, offset, m_consensusID, m_blockNumber, subsetID,
          m_blockHash, m_leaderID, aggregated_commit, aggregated_key,
          m_challenge, GetCommitteeMember(m_leaderID).first)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusChallenge failed.");
    return false;
  }

  // Check the aggregated commit
  if (!aggregated_commit.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid aggregated commit received");
    m_state = ERROR;
    return false;
  }

  // Check the aggregated key
  if (!aggregated_key.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid aggregated key received");
    m_state = ERROR;
    return false;
  }

  // Check the challenge
  if (!m_challenge.Initialized()) {
    LOG_GENERAL(WARNING, "Invalid challenge received");
    m_state = ERROR;
    return false;
  }

  Challenge challenge_verif =
      GetChallenge(m_messageToCosign, aggregated_commit, aggregated_key);

  // If the challenge was gossiped, I may not necessarily be part of the 2/3
  // backups that are part of the bitmap So, I shouldn't change to ERROR state
  // here
  if (!(challenge_verif == m_challenge)) {
    LOG_GENERAL(WARNING, "Generated challenge mismatch");
    return false;
  }

  // Generate response
  // =================

  vector<unsigned char> response = {m_classByte, m_insByte,
                                    static_cast<unsigned char>(returnmsgtype)};
  bool result = GenerateResponseMessage(
      response, MessageOffset::BODY + sizeof(unsigned char), subsetID);
  if (result) {
    // Update internal state
    // =====================

    m_state = nextstate;

    // Unicast to the leader
    // =====================

    P2PComm::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                       response);
  }

  return result;
}

bool ConsensusBackup::ProcessMessageChallenge(
    const vector<unsigned char>& challenge, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageChallengeCore(challenge, offset, PROCESS_CHALLENGE,
                                     RESPONSE, RESPONSE_DONE);
}

bool ConsensusBackup::GenerateResponseMessage(vector<unsigned char>& response,
                                              unsigned int offset,
                                              uint16_t subsetID) {
  LOG_MARKER();

  // Assemble response message body
  // ==============================

  Response r(*m_commitSecret, m_challenge, m_myPrivKey);

  if (!Messenger::SetConsensusResponse(
          response, offset, m_consensusID, m_blockNumber, subsetID, m_blockHash,
          m_myID, r,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusResponse failed.");
    return false;
  }

  return true;
}

bool ConsensusBackup::ProcessMessageCollectiveSigCore(
    const vector<unsigned char>& collectivesig, unsigned int offset,
    Action action, State nextstate) {
  LOG_MARKER();

  // Initial checks
  // ==============
  if (!CheckState(action)) {
    return false;
  }

  // Extract and check collective signature message body
  // ===================================================

  m_responseMap.clear();

  if (!Messenger::GetConsensusCollectiveSig(
          collectivesig, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_leaderID, m_responseMap, m_collectiveSig,
          GetCommitteeMember(m_leaderID).first)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCollectiveSig failed.");
    return false;
  }

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(m_responseMap);
  if (!aggregated_key.Initialized()) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    m_state = ERROR;
    return false;
  }

  if (!Schnorr::GetInstance().Verify(m_messageToCosign, m_collectiveSig,
                                     aggregated_key)) {
    LOG_GENERAL(WARNING, "Collective signature verification failed");
    m_state = ERROR;
    return false;
  }

  // Generate final commit
  // =====================

  bool result = true;

  if (action == PROCESS_COLLECTIVESIG) {
    // First round: consensus over part of message (e.g., DS block header)
    // Second round: consensus over part of message + CS1 + B1
    m_collectiveSig.Serialize(m_messageToCosign, m_messageToCosign.size());
    BitVector::SetBitVector(m_messageToCosign, m_messageToCosign.size(),
                            m_responseMap);

    vector<unsigned char> finalcommit = {
        m_classByte, m_insByte,
        static_cast<unsigned char>(ConsensusMessageType::FINALCOMMIT)};
    result = GenerateCommitMessage(finalcommit,
                                   MessageOffset::BODY + sizeof(unsigned char));
    if (result) {
      // Update internal state
      // =====================

      m_state = nextstate;

      // Save the collective sig over the first round
      m_CS1 = m_collectiveSig;
      m_B1 = m_responseMap;

      // Unicast to the leader
      // =====================
      P2PComm::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                         finalcommit);
    }
  } else {
    // Save the collective sig over the second round
    m_CS2 = m_collectiveSig;
    m_B2 = m_responseMap;

    // Update internal state
    // =====================

    m_state = nextstate;
  }

  return result;
}

bool ConsensusBackup::ProcessMessageCollectiveSig(
    const vector<unsigned char>& collectivesig, unsigned int offset) {
  LOG_MARKER();
  bool collectiveSigResult = ProcessMessageCollectiveSigCore(
      collectivesig, offset, PROCESS_COLLECTIVESIG, FINALCOMMIT_DONE);
  return collectiveSigResult;
}

bool ConsensusBackup::ProcessMessageFinalChallenge(
    const vector<unsigned char>& challenge, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageChallengeCore(challenge, offset, PROCESS_FINALCHALLENGE,
                                     FINALRESPONSE, FINALRESPONSE_DONE);
}

bool ConsensusBackup::ProcessMessageFinalCollectiveSig(
    const vector<unsigned char>& finalcollectivesig, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCollectiveSigCore(finalcollectivesig, offset,
                                         PROCESS_FINALCOLLECTIVESIG, DONE);
}

ConsensusBackup::ConsensusBackup(uint32_t consensus_id, uint64_t block_number,
                                 const vector<unsigned char>& block_hash,
                                 uint16_t node_id, uint16_t leader_id,
                                 const PrivKey& privkey,
                                 const deque<pair<PubKey, Peer>>& committee,
                                 unsigned char class_byte,
                                 unsigned char ins_byte,
                                 MsgContentValidatorFunc msg_validator)
    : ConsensusCommon(consensus_id, block_number, block_hash, node_id, privkey,
                      committee, class_byte, ins_byte),
      m_leaderID(leader_id),
      m_msgContentValidator(msg_validator) {
  LOG_MARKER();
  m_state = INITIAL;
}

ConsensusBackup::~ConsensusBackup() {}

bool ConsensusBackup::ProcessMessage(const vector<unsigned char>& message,
                                     unsigned int offset,
                                     [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  // Incoming message format (from offset): [1-byte consensus message type]
  // [consensus message]

  bool result = false;

  switch (message.at(offset)) {
    case ConsensusMessageType::ANNOUNCE:
      result = ProcessMessageAnnounce(message, offset + 1);
      break;
    case ConsensusMessageType::CONSENSUSFAILURE:
      result = ProcessMessageConsensusFailure(message, offset + 1);
      break;
    case ConsensusMessageType::CHALLENGE:
      result = ProcessMessageChallenge(message, offset + 1);
      break;
    case ConsensusMessageType::COLLECTIVESIG:
      result = ProcessMessageCollectiveSig(message, offset + 1);
      break;
    case ConsensusMessageType::FINALCHALLENGE:
      result = ProcessMessageFinalChallenge(message, offset + 1);
      break;
    case ConsensusMessageType::FINALCOLLECTIVESIG:
      result = ProcessMessageFinalCollectiveSig(message, offset + 1);
      break;
    default:
      LOG_GENERAL(WARNING, "Unknown consensus message received");
  }

  return result;
}

#define MAKE_LITERAL_PAIR(s) \
  { s, #s }

map<ConsensusBackup::Action, string> ConsensusBackup::ActionStrings = {
    MAKE_LITERAL_PAIR(PROCESS_ANNOUNCE), MAKE_LITERAL_PAIR(PROCESS_CHALLENGE),
    MAKE_LITERAL_PAIR(PROCESS_COLLECTIVESIG),
    MAKE_LITERAL_PAIR(PROCESS_FINALCHALLENGE),
    MAKE_LITERAL_PAIR(PROCESS_FINALCOLLECTIVESIG)};

std::string ConsensusBackup::GetActionString(Action action) const {
  return (ActionStrings.find(action) == ActionStrings.end())
             ? "Unknown"
             : ActionStrings.at(action);
}
