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

#include "ConsensusCommon.h"
#include "common/Constants.h"
#include "common/Messages.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libMessage/Messenger.h"
#include "libMessage/ZilliqaMessage.pb.h"
#pragma GCC diagnostic pop
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define MAKE_LITERAL_PAIR(s) \
  { s, #s }

using namespace std;

map<ConsensusCommon::ConsensusErrorCode, std::string>
    ConsensusCommon::CONSENSUSERRORMSG = {
        MAKE_LITERAL_PAIR(NO_ERROR),
        MAKE_LITERAL_PAIR(GENERIC_ERROR),
        MAKE_LITERAL_PAIR(INVALID_DSBLOCK),
        MAKE_LITERAL_PAIR(INVALID_MICROBLOCK),
        MAKE_LITERAL_PAIR(INVALID_FINALBLOCK),
        MAKE_LITERAL_PAIR(INVALID_VIEWCHANGEBLOCK),
        MAKE_LITERAL_PAIR(INVALID_DSBLOCK_VERSION),
        MAKE_LITERAL_PAIR(INVALID_MICROBLOCK_VERSION),
        MAKE_LITERAL_PAIR(INVALID_FINALBLOCK_VERSION),
        MAKE_LITERAL_PAIR(INVALID_FINALBLOCK_NUMBER),
        MAKE_LITERAL_PAIR(INVALID_PREV_FINALBLOCK_HASH),
        MAKE_LITERAL_PAIR(INVALID_VIEWCHANGEBLOCK_VERSION),
        MAKE_LITERAL_PAIR(INVALID_TIMESTAMP),
        MAKE_LITERAL_PAIR(INVALID_BLOCK_HASH),
        MAKE_LITERAL_PAIR(INVALID_MICROBLOCK_ROOT_HASH),
        MAKE_LITERAL_PAIR(MISSING_TXN),
        MAKE_LITERAL_PAIR(WRONG_TXN_ORDER),
        MAKE_LITERAL_PAIR(WRONG_GASUSED),
        MAKE_LITERAL_PAIR(WRONG_REWARDS),
        MAKE_LITERAL_PAIR(INVALID_DS_MICROBLOCK),
        MAKE_LITERAL_PAIR(FINALBLOCK_MISSING_MICROBLOCKS),
        MAKE_LITERAL_PAIR(FINALBLOCK_INVALID_MICROBLOCK_ROOT_HASH),
        MAKE_LITERAL_PAIR(FINALBLOCK_MICROBLOCK_TXNROOT_ERROR),
        MAKE_LITERAL_PAIR(FINALBLOCK_MBS_LEGITIMACY_ERROR),
        MAKE_LITERAL_PAIR(INVALID_MICROBLOCK_STATE_DELTA_HASH),
        MAKE_LITERAL_PAIR(INVALID_MICROBLOCK_SHARD_ID),
        MAKE_LITERAL_PAIR(INVALID_FINALBLOCK_STATE_ROOT),
        MAKE_LITERAL_PAIR(INVALID_FINALBLOCK_STATE_DELTA_HASH),
        MAKE_LITERAL_PAIR(INVALID_COMMHASH)};

ConsensusCommon::ConsensusCommon(uint32_t consensus_id, uint64_t block_number,
                                 const bytes& block_hash, uint16_t my_id,
                                 const PrivKey& privkey,
                                 const deque<pair<PubKey, Peer>>& committee,
                                 unsigned char class_byte,
                                 unsigned char ins_byte)
    : m_consensusErrorCode(NO_ERROR),
      m_consensusID(consensus_id),
      m_blockNumber(block_number),
      m_blockHash(block_hash),
      m_myID(my_id),
      m_myPrivKey(privkey),
      m_committee(committee),
      m_classByte(class_byte),
      m_insByte(ins_byte),
      m_responseMap(committee.size(), false) {}

ConsensusCommon::~ConsensusCommon() {}

Signature ConsensusCommon::SignMessage(const bytes& msg, unsigned int offset,
                                       unsigned int size) {
  LOG_MARKER();

  Signature signature;
  bool result =
      Schnorr::GetInstance().Sign(msg, offset, size, m_myPrivKey,
                                  GetCommitteeMember(m_myID).first, signature);
  if (!result) {
    return Signature();
  }
  return signature;
}

bool ConsensusCommon::VerifyMessage(const bytes& msg, unsigned int offset,
                                    unsigned int size,
                                    const Signature& toverify,
                                    uint16_t peer_id) {
  LOG_MARKER();
  bool result = Schnorr::GetInstance().Verify(
      msg, offset, size, toverify, GetCommitteeMember(peer_id).first);

  if (!result) {
    LOG_GENERAL(INFO, "Peer id: " << peer_id << " pubkey: 0x"
                                  << DataConversion::SerializableToHexStr(
                                         GetCommitteeMember(peer_id).first));
    LOG_GENERAL(INFO, "pubkeys size: " << m_committee.size());
  }
  return result;
}

PubKey ConsensusCommon::AggregateKeys(const vector<bool>& peer_map) {
  LOG_MARKER();

  vector<PubKey> keys;
  deque<pair<PubKey, Peer>>::const_iterator j = m_committee.begin();
  for (unsigned int i = 0; i < peer_map.size(); i++, j++) {
    if (peer_map.at(i)) {
      keys.emplace_back(j->first);
    }
  }
  shared_ptr<PubKey> result = MultiSig::AggregatePubKeys(keys);
  if (result == nullptr) {
    return PubKey();
  }

  return *result;
}

CommitPoint ConsensusCommon::AggregateCommits(
    const vector<CommitPoint>& commits) {
  LOG_MARKER();

  shared_ptr<CommitPoint> aggregated_commit =
      MultiSig::AggregateCommits(commits);
  if (aggregated_commit == nullptr) {
    return CommitPoint();
  }

  return *aggregated_commit;
}

Response ConsensusCommon::AggregateResponses(
    const vector<Response>& responses) {
  LOG_MARKER();

  shared_ptr<Response> aggregated_response =
      MultiSig::AggregateResponses(responses);
  if (aggregated_response == nullptr) {
    return Response();
  }

  return *aggregated_response;
}

Signature ConsensusCommon::AggregateSign(const Challenge& challenge,
                                         const Response& aggregated_response) {
  LOG_MARKER();

  shared_ptr<Signature> result =
      MultiSig::AggregateSign(challenge, aggregated_response);
  if (result == nullptr) {
    return Signature();
  }

  return *result;
}

Challenge ConsensusCommon::GetChallenge(const bytes& msg,
                                        const CommitPoint& aggregated_commit,
                                        const PubKey& aggregated_key) {
  LOG_MARKER();

  return Challenge(aggregated_commit, aggregated_key, msg);
}

pair<PubKey, Peer> ConsensusCommon::GetCommitteeMember(
    const unsigned int index) {
  if (m_committee.size() <= index) {
    LOG_GENERAL(WARNING, "Committee size " << m_committee.size() << " <= index "
                                           << index
                                           << ". Returning default values.");
    return make_pair(PubKey(), Peer());
  }
  return m_committee.at(index);
}

ConsensusCommon::State ConsensusCommon::GetState() const { return m_state; }

bool ConsensusCommon::GetConsensusID(const bytes& message,
                                     const unsigned int offset,
                                     uint32_t& consensusID) const {
  if (message.size() > offset) {
    switch (message.at(offset)) {
      case ConsensusMessageType::ANNOUNCE:
        return Messenger::GetConsensusID<ZilliqaMessage::ConsensusAnnouncement>(
            message, offset + 1, consensusID);
      case ConsensusMessageType::CONSENSUSFAILURE:
        return true;
      case ConsensusMessageType::COMMIT:
      case ConsensusMessageType::FINALCOMMIT:
        return Messenger::GetConsensusID<ZilliqaMessage::ConsensusCommit>(
            message, offset + 1, consensusID);
      case ConsensusMessageType::COMMITFAILURE:
        return Messenger::GetConsensusID<
            ZilliqaMessage::ConsensusCommitFailure>(message, offset + 1,
                                                    consensusID);
      case ConsensusMessageType::CHALLENGE:
      case ConsensusMessageType::FINALCHALLENGE:
        return Messenger::GetConsensusID<ZilliqaMessage::ConsensusChallenge>(
            message, offset + 1, consensusID);
      case ConsensusMessageType::RESPONSE:
      case ConsensusMessageType::FINALRESPONSE:
        return Messenger::GetConsensusID<ZilliqaMessage::ConsensusResponse>(
            message, offset + 1, consensusID);
      case ConsensusMessageType::COLLECTIVESIG:
      case ConsensusMessageType::FINALCOLLECTIVESIG:
        return Messenger::GetConsensusID<
            ZilliqaMessage::ConsensusCollectiveSig>(message, offset + 1,
                                                    consensusID);
      default:
        LOG_GENERAL(WARNING, "Unknown consensus message received");
        break;
    }
  } else {
    LOG_GENERAL(WARNING, "Consensus message offset " << offset << " >= size "
                                                     << message.size());
  }

  return false;
}

ConsensusCommon::ConsensusErrorCode ConsensusCommon::GetConsensusErrorCode()
    const {
  return m_consensusErrorCode;
}

std::string ConsensusCommon::GetConsensusErrorMsg() const {
  if (CONSENSUSERRORMSG.find(m_consensusErrorCode) == CONSENSUSERRORMSG.end()) {
    return "Error. No such error code.";
  } else {
    return CONSENSUSERRORMSG.at(m_consensusErrorCode);
  }
}

void ConsensusCommon::SetConsensusErrorCode(
    ConsensusCommon::ConsensusErrorCode ErrorCode) {
  m_consensusErrorCode = ErrorCode;
}

void ConsensusCommon::RecoveryAndProcessFromANewState(State newState) {
  m_state = newState;
}

const Signature& ConsensusCommon::GetCS1() const {
  if (m_state != DONE) {
    LOG_GENERAL(WARNING,
                "Retrieving collectivesig when consensus is still ongoing");
  }

  return m_CS1;
}

const vector<bool>& ConsensusCommon::GetB1() const {
  if (m_state != DONE) {
    LOG_GENERAL(WARNING,
                "Retrieving collectivesig bit map when consensus is "
                "still ongoing");
  }

  return m_B1;
}

const Signature& ConsensusCommon::GetCS2() const {
  if (m_state != DONE) {
    LOG_GENERAL(WARNING,
                "Retrieving collectivesig when consensus is still ongoing");
  }

  return m_CS2;
}

const vector<bool>& ConsensusCommon::GetB2() const {
  if (m_state != DONE) {
    LOG_GENERAL(WARNING,
                "Retrieving collectivesig bit map when consensus is "
                "still ongoing");
  }

  return m_B2;
}

unsigned int ConsensusCommon::NumForConsensus(unsigned int shardSize) {
  return ceil(shardSize * TOLERANCE_FRACTION);
}

bool ConsensusCommon::CanProcessMessage(const bytes& message,
                                        unsigned int offset) {
  if (message.size() <= offset) {
    LOG_GENERAL(WARNING, "Consensus message offset " << offset << " >= size "
                                                     << message.size());
    return false;
  }

  const unsigned char messageType = message.at(offset);

  if (messageType == ConsensusMessageType::COLLECTIVESIG) {
    if (m_state == INITIAL) {
      LOG_GENERAL(WARNING,
                  "Processing collectivesig but announcement not yet "
                  "received. m_state "
                      << GetStateString());
      return false;
    }
  } else if (messageType == ConsensusMessageType::FINALCOLLECTIVESIG) {
    if (m_state == INITIAL || m_state == COMMIT_DONE ||
        m_state == RESPONSE_DONE) {
      LOG_GENERAL(WARNING,
                  "Processing final collectivesig but cosig1 not yet "
                  "received. m_state "
                      << GetStateString());
      return false;
    }
  }

  return true;
}

map<ConsensusCommon::State, string> ConsensusCommon::ConsensusStateStrings = {
    MAKE_LITERAL_PAIR(INITIAL),
    MAKE_LITERAL_PAIR(ANNOUNCE_DONE),
    MAKE_LITERAL_PAIR(COMMIT_DONE),
    MAKE_LITERAL_PAIR(CHALLENGE_DONE),
    MAKE_LITERAL_PAIR(RESPONSE_DONE),
    MAKE_LITERAL_PAIR(COLLECTIVESIG_DONE),
    MAKE_LITERAL_PAIR(FINALCOMMIT_DONE),
    MAKE_LITERAL_PAIR(FINALCHALLENGE_DONE),
    MAKE_LITERAL_PAIR(FINALRESPONSE_DONE),
    MAKE_LITERAL_PAIR(DONE),
    MAKE_LITERAL_PAIR(ERROR)};

string ConsensusCommon::GetStateString() const {
  if (ConsensusStateStrings.find(m_state) == ConsensusStateStrings.end()) {
    return "Unknown";
  } else {
    return ConsensusStateStrings.at(m_state);
  }
}

string ConsensusCommon::GetStateString(const State state) const {
  if (ConsensusStateStrings.find(state) == ConsensusStateStrings.end()) {
    return "Unknown";
  } else {
    return ConsensusStateStrings.at(state);
  }
}
