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

#include "ConsensusBackup.h"

#include <utility>
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
       {COMMIT_DONE, PROCESS_FINALCOLLECTIVESIG},
       {RESPONSE_DONE, PROCESS_CHALLENGE},
       {RESPONSE_DONE, PROCESS_COLLECTIVESIG},
       {RESPONSE_DONE, PROCESS_FINALCOLLECTIVESIG},
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
    LOG_GENERAL(WARNING, GetActionString(action)
                             << " not allowed in " << GetStateString());
    return false;
  }

  return true;
}

bool ConsensusBackup::ProcessMessageAnnounce(const bytes& announcement,
                                             unsigned int offset) {
  LOG_MARKER();

  // Initial checks
  // ==============

  if (!CheckState(PROCESS_ANNOUNCE)) {
    return false;
  }

  // Extract and check announce message body
  // =======================================

  bytes errorMsg;
  // Following will get us m_prePrepMicroblock.
  MsgContentValidatorFunc func = m_msgContentValidator;
  if (m_prePrepMsgContentValidator) {
    func = m_prePrepMsgContentValidator;
  }
  if (!func(announcement, offset, errorMsg, m_consensusID, m_blockNumber,
            m_blockHash, m_leaderID, GetCommitteeMember(m_leaderID).first,
            m_messageToCosign)) {
    LOG_GENERAL(WARNING, "Message validation failed");

    if (!errorMsg.empty()) {
      LOG_GENERAL(WARNING, "Sending commit failure to leader");

      bytes commitFailureMsg = {
          m_classByte, m_insByte,
          static_cast<uint8_t>(ConsensusMessageType::COMMITFAILURE)};

      bool result = GenerateCommitFailureMessage(
          commitFailureMsg, MessageOffset::BODY + sizeof(uint8_t), errorMsg);

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

    return false;
  }

  // Validation of round 1 announcement is successful. Start executing
  // background task if any.
  if (m_postPrePrepContentValidation) {
    m_postPrePrepContentValidation();
  }

  // Generate commit
  // ===============

  bytes commit = {m_classByte, m_insByte,
                  static_cast<uint8_t>(ConsensusMessageType::COMMIT)};

  bool result =
      GenerateCommitMessage(commit, MessageOffset::BODY + sizeof(uint8_t));
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

bool ConsensusBackup::ProcessMessageConsensusFailure(const bytes& announcement,
                                                     unsigned int offset) {
  LOG_MARKER();

  if (!Messenger::GetConsensusConsensusFailure(
          announcement, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_leaderID, GetCommitteeMember(m_leaderID).first)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusConsensusFailure failed");
    return false;
  }

  m_state = INITIAL;

  return true;
}

bool ConsensusBackup::GenerateCommitFailureMessage(bytes& commitFailure,
                                                   unsigned int offset,
                                                   const bytes& errorMsg) {
  LOG_MARKER();

  if (!Messenger::SetConsensusCommitFailure(
          commitFailure, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_myID, errorMsg,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCommitFailure failed");
    return false;
  }

  return true;
}

bool ConsensusBackup::GenerateCommitMessage(bytes& commit,
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
          *m_commitPoint, CommitPointHash(*m_commitPoint),
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCommit failed");
    return false;
  }

  return true;
}

bool ConsensusBackup::ProcessMessageChallengeCore(
    const bytes& challenge, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate) {
  LOG_MARKER();

  // Initial checks
  // ==============

  if (!CheckState(action)) {
    return false;
  }

  // Extract and check challenge message body
  // ========================================

  vector<ChallengeSubsetInfo> challengeSubsetInfo;
  vector<ResponseSubsetInfo> responseSubsetInfo;

  if (!Messenger::GetConsensusChallenge(challenge, offset, m_consensusID,
                                        m_blockNumber, m_blockHash, m_leaderID,
                                        challengeSubsetInfo,
                                        GetCommitteeMember(m_leaderID).first)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusChallenge failed");
    return false;
  }

  for (unsigned int subsetID = 0; subsetID < challengeSubsetInfo.size();
       subsetID++) {
    // Check the aggregated commit
    if (!challengeSubsetInfo.at(subsetID).aggregatedCommit.Initialized()) {
      LOG_GENERAL(WARNING,
                  "[Subset " << subsetID << "] Invalid aggregated commit");
      m_state = ERROR;
      return false;
    }

    // Check the challenge
    if (!challengeSubsetInfo.at(subsetID).challenge.Initialized()) {
      LOG_GENERAL(WARNING, "[Subset " << subsetID << "] Invalid challenge");
      m_state = ERROR;
      return false;
    }

    Challenge challenge_verif = GetChallenge(
        m_messageToCosign, challengeSubsetInfo.at(subsetID).aggregatedCommit,
        challengeSubsetInfo.at(subsetID).aggregatedKey);

    if (!(challenge_verif == challengeSubsetInfo.at(subsetID).challenge)) {
      LOG_GENERAL(WARNING,
                  "[Subset " << subsetID << "] Generated challenge mismatch");
      m_state = ERROR;
      return false;
    }

    ResponseSubsetInfo rsi;

    rsi.response =
        Response(*m_commitSecret, challengeSubsetInfo.at(subsetID).challenge,
                 m_myPrivKey);

    responseSubsetInfo.emplace_back(rsi);
  }

  // Generate response
  // =================

  bytes response = {m_classByte, m_insByte,
                    static_cast<uint8_t>(returnmsgtype)};
  if (GenerateResponseMessage(response, MessageOffset::BODY + sizeof(uint8_t),
                              responseSubsetInfo)) {
    // Update internal state
    // =====================

    m_state = nextstate;

    // Unicast to the leader
    // =====================

    P2PComm::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                       response);

    return true;
  }

  return false;
}

bool ConsensusBackup::ProcessMessageChallenge(const bytes& challenge,
                                              unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageChallengeCore(challenge, offset, PROCESS_CHALLENGE,
                                     RESPONSE, RESPONSE_DONE);
}

bool ConsensusBackup::GenerateResponseMessage(
    bytes& response, unsigned int offset,
    const vector<ResponseSubsetInfo>& subsetInfo) {
  LOG_MARKER();

  // Assemble response message body
  // ==============================

  if (!Messenger::SetConsensusResponse(
          response, offset, m_consensusID, m_blockNumber, m_blockHash, m_myID,
          subsetInfo,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusResponse failed");
    return false;
  }

  return true;
}

bool ConsensusBackup::ProcessMessageCollectiveSigCore(
    const bytes& collectivesig, unsigned int offset, Action action,
    State nextstate) {
  LOG_MARKER();

  // Initial checks
  // ==============
  if (!CheckState(action)) {
    return false;
  }

  // Extract and check collective signature message body
  // ===================================================

  m_responseMap.clear();

  bytes newAnnouncementMsg;
  if (!Messenger::GetConsensusCollectiveSig(
          collectivesig, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_leaderID, m_responseMap, m_collectiveSig,
          GetCommitteeMember(m_leaderID).first, newAnnouncementMsg)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCollectiveSig failed");
    return false;
  }

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(m_responseMap);

  if (!MultiSig::MultiSigVerify(m_messageToCosign, m_collectiveSig,
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
    // Second round: consensus over part of new message + (CS1 + B1 for part of
    // older message)
    if (m_readinessFunc) {
      // wait for readiness signal to start with collective sig processing.
      if (!m_readinessFunc()) {
        return false;
      }
    }
    if (!newAnnouncementMsg.empty()) {
      bytes errorMsg;
      // Following will get us m_microblock.
      bytes newMessageToCosig;
      if (!m_msgContentValidator(
              newAnnouncementMsg, offset, errorMsg, m_consensusID,
              m_blockNumber, m_blockHash, m_leaderID,
              GetCommitteeMember(m_leaderID).first, m_messageToCosign)) {
        LOG_GENERAL(WARNING, "Message validation failed");
        m_state = ERROR;
        return false;
      }
    }
    m_collectiveSig.Serialize(m_messageToCosign, m_messageToCosign.size());
    BitVector::SetBitVector(m_messageToCosign, m_messageToCosign.size(),
                            m_responseMap);

    bytes finalcommit = {
        m_classByte, m_insByte,
        static_cast<uint8_t>(ConsensusMessageType::FINALCOMMIT)};
    result = GenerateCommitMessage(finalcommit,
                                   MessageOffset::BODY + sizeof(uint8_t));
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

bool ConsensusBackup::ProcessMessageCollectiveSig(const bytes& collectivesig,
                                                  unsigned int offset) {
  LOG_MARKER();
  bool collectiveSigResult = ProcessMessageCollectiveSigCore(
      collectivesig, offset, PROCESS_COLLECTIVESIG, FINALCOMMIT_DONE);
  return collectiveSigResult;
}

bool ConsensusBackup::ProcessMessageFinalChallenge(const bytes& challenge,
                                                   unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageChallengeCore(challenge, offset, PROCESS_FINALCHALLENGE,
                                     FINALRESPONSE, FINALRESPONSE_DONE);
}

bool ConsensusBackup::ProcessMessageFinalCollectiveSig(
    const bytes& finalcollectivesig, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCollectiveSigCore(finalcollectivesig, offset,
                                         PROCESS_FINALCOLLECTIVESIG, DONE);
}

ConsensusBackup::ConsensusBackup(
    uint32_t consensus_id, uint64_t block_number, const bytes& block_hash,
    uint16_t node_id, uint16_t leader_id, const PrivKey& privkey,
    const DequeOfNode& committee, uint8_t class_byte, uint8_t ins_byte,
    MsgContentValidatorFunc msg_validator,
    MsgContentValidatorFunc preprep_msg_validator,
    PostPrePrepValidationFunc post_preprep_validation,
    CollectiveSigReadinessFunc collsig_readiness_func)
    : ConsensusCommon(consensus_id, block_number, block_hash, node_id, privkey,
                      committee, class_byte, ins_byte),
      m_leaderID(leader_id),
      m_msgContentValidator(move(msg_validator)),
      m_prePrepMsgContentValidator(move(preprep_msg_validator)),
      m_postPrePrepContentValidation(move(post_preprep_validation)),
      m_readinessFunc(move(collsig_readiness_func)) {
  LOG_MARKER();
  m_state = INITIAL;

  LOG_GENERAL(INFO, "Consensus ID = " << m_consensusID);
  LOG_GENERAL(INFO, "Leader ID    = " << m_leaderID);
  LOG_GENERAL(INFO, "My ID        = " << m_myID);
}

ConsensusBackup::~ConsensusBackup() {}

bool ConsensusBackup::ProcessMessage(const bytes& message, unsigned int offset,
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
      LOG_GENERAL(WARNING,
                  "Unknown msg type " << (unsigned int)message.at(offset));
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
             ? "UNKNOWN"
             : ActionStrings.at(action);
}
