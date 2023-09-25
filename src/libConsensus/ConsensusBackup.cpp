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
#include "libMetrics/Api.h"
#include "libMetrics/TracedIds.h"
#include "libNetwork/P2P.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

#include <boost/algorithm/string.hpp>

using namespace std;

namespace zil {
namespace local {

class BackupVariables {
  int consensusState = -1;
  int consensusError = 0;

 public:
  std::unique_ptr<Z_I64GAUGE> temp;

  void SetConsensusState(int state) {
    Init();
    consensusState = state;
  }

  void AddConsensusError(int count) {
    Init();
    consensusError += count;
  }

  void Init() {
    if (!temp) {
      temp =
          std::make_unique<Z_I64GAUGE>(Z_FL::BLOCKS, "consensus.backup.gauge",
                                       "Consensus bacup state", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(consensusState, {{"counter", "ConsensusState"}});
        result.Set(consensusError, {{"counter", "ConsensusError"}});
      });
    }
  }
};

static BackupVariables variables{};

}  // namespace local
}  // namespace zil

bool ConsensusBackup::CheckState(Action action) {
  static const std::multimap<ConsensusCommon::State, Action> ACTIONS_FOR_STATE =
      {{INITIAL, PROCESS_ANNOUNCE},
       {COMMIT_DONE, PROCESS_CHALLENGE},
       {COMMIT_DONE, PROCESS_COLLECTIVESIG},
       {COMMIT_DONE, PROCESS_FINALCOLLECTIVESIG},
       {RESPONSE_DONE, PROCESS_COLLECTIVESIG},
       {RESPONSE_DONE, PROCESS_FINALCOLLECTIVESIG},
       {FINALCOMMIT_DONE, PROCESS_FINALCHALLENGE},
       {FINALCOMMIT_DONE, PROCESS_FINALCOLLECTIVESIG},
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

bool ConsensusBackup::ProcessMessageAnnounce(const zbytes& announcement,
                                             unsigned int offset) {
  LOG_MARKER();

  auto span = zil::trace::Tracing::CreateChildSpanOfRemoteTrace(
      zil::trace::FilterClass::NODE, "Announce",
      TracedIds::GetInstance().GetConsensusSpanIds());

  // Initial checks
  // ==============

  if (!CheckState(PROCESS_ANNOUNCE)) {
    return false;
  }

  // Extract and check announce message body
  // =======================================

  zbytes errorMsg;
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

      zbytes commitFailureMsg = {
          m_classByte, m_insByte,
          static_cast<uint8_t>(ConsensusMessageType::COMMITFAILURE)};

      bool result = GenerateCommitFailureMessage(
          commitFailureMsg, MessageOffset::BODY + sizeof(uint8_t), errorMsg);

      if (result) {
        // Update internal state
        // =====================
        m_state = ERROR;
        zil::local::variables.SetConsensusState(int(m_state));
        zil::local::variables.AddConsensusError(1);

        // Unicast to the leader
        // =====================
        LOG_GENERAL(WARNING,
                    "Uni-casting response to leader (message announce)");
        zil::p2p::GetInstance().SendMessage(
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

  zbytes commit = {m_classByte, m_insByte,
                   static_cast<uint8_t>(ConsensusMessageType::COMMIT)};

  bool result =
      GenerateCommitMessage(commit, MessageOffset::BODY + sizeof(uint8_t));
  if (result) {
    // Update internal state
    // =====================
    m_state = COMMIT_DONE;
    zil::local::variables.SetConsensusState(int(m_state));

    // Unicast to the leader
    // =====================
    LOG_GENERAL(WARNING, "Uni-casting response to leader (message announce2)");
    zil::p2p::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                        commit);
  }
  return result;
}

bool ConsensusBackup::ProcessMessageConsensusFailure(const zbytes& announcement,
                                                     unsigned int offset) {
  LOG_MARKER();

  auto span = zil::trace::Tracing::CreateChildSpanOfRemoteTrace(
      zil::trace::FilterClass::NODE, "ConsensusFailure",
      TracedIds::GetInstance().GetConsensusSpanIds());
  if (!Messenger::GetConsensusConsensusFailure(
          announcement, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_leaderID, GetCommitteeMember(m_leaderID).first)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusConsensusFailure failed");
    return false;
  }

  m_state = INITIAL;
  zil::local::variables.SetConsensusState(int(m_state));

  return true;
}

bool ConsensusBackup::GenerateCommitFailureMessage(zbytes& commitFailure,
                                                   unsigned int offset,
                                                   const zbytes& errorMsg) {
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

bool ConsensusBackup::GenerateCommitMessage(zbytes& commit,
                                            unsigned int offset) {
  LOG_MARKER();

  // Generate new commit
  // ===================
  m_commitInfo.clear();
  m_commitSecrets.clear();

  for (unsigned int i = 0; i < m_numOfSubsets; i++) {
    CommitInfo ci;
    CommitSecret cs;
    ci.commit = CommitPoint(cs);
    ci.hash = CommitPointHash(ci.commit);
    m_commitInfo.emplace_back(ci);
    m_commitSecrets.emplace_back(cs);
  }

  // Assemble commit message body
  // ============================

  if (!Messenger::SetConsensusCommit(
          commit, offset, m_consensusID, m_blockNumber, m_blockHash, m_myID,
          m_commitInfo,
          make_pair(m_myPrivKey, GetCommitteeMember(m_myID).first))) {
    LOG_GENERAL(WARNING, "Messenger::SetConsensusCommit failed");
    return false;
  }

  return true;
}

bool ConsensusBackup::ProcessMessageChallengeCore(
    const zbytes& challenge, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate,
    std::string_view spanName) {
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

  auto span = zil::trace::Tracing::CreateChildSpanOfRemoteTrace(
      zil::trace::FilterClass::NODE, spanName,
      TracedIds::GetInstance().GetConsensusSpanIds());

  for (unsigned int subsetID = 0; subsetID < challengeSubsetInfo.size();
       subsetID++) {
    // Check the aggregated commit
    if (!challengeSubsetInfo.at(subsetID).aggregatedCommit.Initialized()) {
      LOG_GENERAL(WARNING,
                  "[Subset " << subsetID << "] Invalid aggregated commit");
      m_state = ERROR;
      zil::local::variables.SetConsensusState(int(m_state));
      zil::local::variables.AddConsensusError(1);
      return false;
    }

    // Check the challenge
    if (!challengeSubsetInfo.at(subsetID).challenge.Initialized()) {
      LOG_GENERAL(WARNING, "[Subset " << subsetID << "] Invalid challenge");
      m_state = ERROR;
      zil::local::variables.SetConsensusState(int(m_state));
      zil::local::variables.AddConsensusError(1);
      return false;
    }

    Challenge challenge_verif = GetChallenge(
        m_messageToCosign, challengeSubsetInfo.at(subsetID).aggregatedCommit,
        challengeSubsetInfo.at(subsetID).aggregatedKey);

    if (!(challenge_verif == challengeSubsetInfo.at(subsetID).challenge)) {
      LOG_GENERAL(WARNING,
                  "[Subset " << subsetID << "] Generated challenge mismatch");
      m_state = ERROR;
      zil::local::variables.SetConsensusState(int(m_state));
      zil::local::variables.AddConsensusError(1);
      return false;
    }

    ResponseSubsetInfo rsi;

    rsi.response =
        Response(m_commitSecrets.at(subsetID),
                 challengeSubsetInfo.at(subsetID).challenge, m_myPrivKey);

    responseSubsetInfo.emplace_back(rsi);
  }

  // Generate response
  // =================

  zbytes response = {m_classByte, m_insByte,
                     static_cast<uint8_t>(returnmsgtype)};
  if (GenerateResponseMessage(response, MessageOffset::BODY + sizeof(uint8_t),
                              responseSubsetInfo)) {
    // Update internal state
    // =====================

    m_state = nextstate;
    zil::local::variables.SetConsensusState(int(m_state));

    // Unicast to the leader
    // =====================
    LOG_GENERAL(WARNING, "Uni-casting response to leader (message challenge)");
    zil::p2p::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                        response);

    return true;
  }

  return false;
}

bool ConsensusBackup::ProcessMessageChallenge(const zbytes& challenge,
                                              unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageChallengeCore(challenge, offset, PROCESS_CHALLENGE,
                                     RESPONSE, RESPONSE_DONE, "Challenge");
}

bool ConsensusBackup::GenerateResponseMessage(
    zbytes& response, unsigned int offset,
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
    const zbytes& collectivesig, unsigned int offset, Action action,
    State nextstate, std::string_view spanName) {
  LOG_MARKER();

  // Initial checks
  // ==============
  if (!CheckState(action)) {
    return false;
  }

  // Extract and check collective signature message body
  // ===================================================

  m_responseMap.clear();

  zbytes newAnnouncementMsg;
  if (!Messenger::GetConsensusCollectiveSig(
          collectivesig, offset, m_consensusID, m_blockNumber, m_blockHash,
          m_leaderID, m_responseMap, m_collectiveSig,
          GetCommitteeMember(m_leaderID).first, newAnnouncementMsg)) {
    LOG_GENERAL(WARNING, "Messenger::GetConsensusCollectiveSig failed");
    return false;
  }

  auto span = zil::trace::Tracing::CreateChildSpanOfRemoteTrace(
      zil::trace::FilterClass::NODE, spanName,
      TracedIds::GetInstance().GetConsensusSpanIds());

  // Aggregate keys
  PubKey aggregated_key = AggregateKeys(m_responseMap);

  if (!MultiSig::MultiSigVerify(m_messageToCosign, m_collectiveSig,
                                aggregated_key)) {
    LOG_GENERAL(WARNING, "Collective signature verification failed");
    m_state = ERROR;
    zil::local::variables.SetConsensusState(int(m_state));
    zil::local::variables.AddConsensusError(1);
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
      zbytes errorMsg;
      // Following will get us m_microblock.
      zbytes newMessageToCosig;
      if (!m_msgContentValidator(
              newAnnouncementMsg, offset, errorMsg, m_consensusID,
              m_blockNumber, m_blockHash, m_leaderID,
              GetCommitteeMember(m_leaderID).first, m_messageToCosign)) {
        LOG_GENERAL(WARNING, "Message validation failed");
        m_state = ERROR;
        zil::local::variables.SetConsensusState(int(m_state));
        zil::local::variables.AddConsensusError(1);
        return false;
      }
    }
    m_collectiveSig.Serialize(m_messageToCosign, m_messageToCosign.size());
    BitVector::SetBitVector(m_messageToCosign, m_messageToCosign.size(),
                            m_responseMap);

    zbytes finalcommit = {
        m_classByte, m_insByte,
        static_cast<uint8_t>(ConsensusMessageType::FINALCOMMIT)};
    result = GenerateCommitMessage(finalcommit,
                                   MessageOffset::BODY + sizeof(uint8_t));
    if (result) {
      // Update internal state
      // =====================

      m_state = nextstate;
      zil::local::variables.SetConsensusState(int(m_state));

      // Save the collective sig over the first round
      m_CS1 = m_collectiveSig;
      m_B1 = m_responseMap;

      // Unicast to the leader
      // =====================
      LOG_GENERAL(
          WARNING,
          "Uni-casting response to leader (message collective sig core)");
      zil::p2p::GetInstance().SendMessage(GetCommitteeMember(m_leaderID).second,
                                          finalcommit);
    }
  } else {
    // Save the collective sig over the second round
    m_CS2 = m_collectiveSig;
    m_B2 = m_responseMap;

    // Update internal state
    // =====================

    m_state = nextstate;
    zil::local::variables.SetConsensusState(int(m_state));
  }

  return result;
}

bool ConsensusBackup::ProcessMessageCollectiveSig(const zbytes& collectivesig,
                                                  unsigned int offset) {
  LOG_MARKER();
  bool collectiveSigResult = ProcessMessageCollectiveSigCore(
      collectivesig, offset, PROCESS_COLLECTIVESIG, FINALCOMMIT_DONE,
      "CollectiveSig");
  return collectiveSigResult;
}

bool ConsensusBackup::ProcessMessageFinalChallenge(const zbytes& challenge,
                                                   unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageChallengeCore(challenge, offset, PROCESS_FINALCHALLENGE,
                                     FINALRESPONSE, FINALRESPONSE_DONE,
                                     "FinalChallenge");
}

bool ConsensusBackup::ProcessMessageFinalCollectiveSig(
    const zbytes& finalcollectivesig, unsigned int offset) {
  LOG_MARKER();
  return ProcessMessageCollectiveSigCore(finalcollectivesig, offset,
                                         PROCESS_FINALCOLLECTIVESIG, DONE,
                                         "FinalCollectiveSig");
}

ConsensusBackup::ConsensusBackup(
    uint32_t consensus_id, uint64_t block_number, const zbytes& block_hash,
    uint16_t node_id, uint16_t leader_id, const PrivKey& privkey,
    const DequeOfNode& committee, uint8_t class_byte, uint8_t ins_byte,
    MsgContentValidatorFunc msg_validator,
    MsgContentValidatorFunc preprep_msg_validator,
    PostPrePrepValidationFunc post_preprep_validation,
    CollectiveSigReadinessFunc collsig_readiness_func, bool isDS)
    : ConsensusCommon(consensus_id, block_number, block_hash, node_id, privkey,
                      committee, class_byte, ins_byte, isDS),
      m_leaderID(leader_id),
      m_msgContentValidator(std::move(msg_validator)),
      m_prePrepMsgContentValidator(std::move(preprep_msg_validator)),
      m_postPrePrepContentValidation(std::move(post_preprep_validation)),
      m_readinessFunc(std::move(collsig_readiness_func)) {
  LOG_MARKER();
  m_state = INITIAL;
  zil::local::variables.SetConsensusState(int(m_state));

  LOG_GENERAL(INFO, "Consensus ID = " << m_consensusID);
  LOG_GENERAL(INFO, "Leader ID    = " << m_leaderID);
  LOG_GENERAL(INFO, "My ID        = " << m_myID);

  auto span = zil::trace::Tracing::CreateChildSpanOfRemoteTrace(
      zil::trace::FilterClass::NODE, "Consensus",
      TracedIds::GetInstance().GetCurrentEpochSpanIds());
  span.SetAttribute("consensus.role", "backup");
  span.SetAttribute("consensus.id", static_cast<uint64_t>(m_consensusID));
  span.SetAttribute("consensus.leader_id", static_cast<uint64_t>(m_leaderID));
  span.SetAttribute("consensus.node_id", static_cast<uint64_t>(m_myID));
  span.SetAttribute("consensus.block_number", m_blockNumber);
  TracedIds::GetInstance().SetConsensusSpanIds(span.GetIds());
}

ConsensusBackup::~ConsensusBackup() {}

bool ConsensusBackup::ProcessMessage(const zbytes& message, unsigned int offset,
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
