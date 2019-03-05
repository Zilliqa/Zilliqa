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

#ifndef __CONSENSUSCOMMON_H__
#define __CONSENSUSCOMMON_H__

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "libCrypto/MultiSig.h"
#include "libNetwork/ShardStruct.h"
#include "libUtils/TimeLockedFunction.h"

struct ChallengeSubsetInfo {
  CommitPoint aggregatedCommit;
  PubKey aggregatedKey;
  Challenge challenge;
};

struct ResponseSubsetInfo {
  Response response;
};

/// Implements base functionality shared between all consensus committee members
class ConsensusCommon {
 public:
  enum State {
    INITIAL = 0x00,
    ANNOUNCE_DONE,
    COMMIT_DONE,
    CHALLENGE_DONE,
    RESPONSE_DONE,
    COLLECTIVESIG_DONE,
    FINALCOMMIT_DONE,
    FINALCHALLENGE_DONE,
    FINALRESPONSE_DONE,
    DONE,
    ERROR
  };

  enum ConsensusErrorCode : uint16_t {
    NO_ERROR = 0x00,
    GENERIC_ERROR,
    INVALID_DSBLOCK,
    INVALID_MICROBLOCK,
    INVALID_FINALBLOCK,
    INVALID_VIEWCHANGEBLOCK,
    INVALID_DSBLOCK_VERSION,
    INVALID_MICROBLOCK_VERSION,
    INVALID_FINALBLOCK_VERSION,
    INVALID_FINALBLOCK_NUMBER,
    INVALID_PREV_FINALBLOCK_HASH,
    INVALID_VIEWCHANGEBLOCK_VERSION,
    INVALID_TIMESTAMP,
    INVALID_BLOCK_HASH,
    INVALID_MICROBLOCK_ROOT_HASH,
    MISSING_TXN,
    WRONG_TXN_ORDER,
    WRONG_GASUSED,
    WRONG_REWARDS,
    FINALBLOCK_MISSING_MICROBLOCKS,
    FINALBLOCK_INVALID_MICROBLOCK_ROOT_HASH,
    FINALBLOCK_MICROBLOCK_TXNROOT_ERROR,
    FINALBLOCK_MBS_LEGITIMACY_ERROR,
    INVALID_DS_MICROBLOCK,
    INVALID_MICROBLOCK_STATE_DELTA_HASH,
    INVALID_MICROBLOCK_SHARD_ID,
    INVALID_MICROBLOCK_TRAN_RECEIPT_HASH,
    INVALID_FINALBLOCK_STATE_ROOT,
    INVALID_FINALBLOCK_STATE_DELTA_HASH,
    INVALID_COMMHASH
  };

  static std::map<ConsensusErrorCode, std::string> CONSENSUSERRORMSG;

 protected:
  enum ConsensusMessageType : unsigned char {
    ANNOUNCE = 0x00,
    COMMIT = 0x01,
    CHALLENGE = 0x02,
    RESPONSE = 0x03,
    COLLECTIVESIG = 0x04,
    FINALCOMMIT = 0x05,
    FINALCHALLENGE = 0x06,
    FINALRESPONSE = 0x07,
    FINALCOLLECTIVESIG = 0x08,
    COMMITFAILURE = 0x09,
    CONSENSUSFAILURE = 0x10,
  };

  /// State of the active consensus session.
  std::atomic<State> m_state;

  /// State of the active consensus session.
  ConsensusErrorCode m_consensusErrorCode;

  /// The unique ID assigned to the active consensus session.
  uint32_t m_consensusID;

  /// The latest final block number
  uint64_t m_blockNumber;

  /// [TODO] The unique block hash assigned to the active consensus session.
  bytes m_blockHash;

  /// The ID assigned to this peer (equal to its index in the peer table).
  uint16_t m_myID;

  /// Private key of this peer.
  PrivKey m_myPrivKey;

  /// List of <public keys, peers> for the committee.
  DequeOfNode m_committee;

  /// The payload segment to be co-signed by the committee.
  bytes m_messageToCosign;

  /// The class byte value for the next consensus message to be composed.
  unsigned char m_classByte;

  /// The instruction byte value for the next consensus message to be composed.
  unsigned char m_insByte;

  /// Generated collective signature
  Signature m_collectiveSig;

  /// Response map for the generated collective signature
  std::vector<bool> m_responseMap;

  /// Co-sig for first round
  Signature m_CS1;

  /// Co-sig bitmap for first round
  std::vector<bool> m_B1;

  /// Co-sig for second round
  Signature m_CS2;

  /// Co-sig bitmap for second round
  std::vector<bool> m_B2;

  /// Generated commit secret
  std::shared_ptr<CommitSecret> m_commitSecret;

  /// Generated commit point
  std::shared_ptr<CommitPoint> m_commitPoint;

  /// Constructor.
  ConsensusCommon(uint32_t consensus_id, uint64_t block_number,
                  const bytes& block_hash, uint16_t my_id,
                  const PrivKey& privkey, const DequeOfNode& committee,
                  unsigned char class_byte, unsigned char ins_byte);

  /// Destructor.
  virtual ~ConsensusCommon();

  /// Generates the signature over a consensus message.
  Signature SignMessage(const bytes& msg, unsigned int offset,
                        unsigned int size);

  /// Verifies the signature attached to a consensus message.
  bool VerifyMessage(const bytes& msg, unsigned int offset, unsigned int size,
                     const Signature& toverify, uint16_t peer_id);

  /// Aggregates public keys according to the response map.
  PubKey AggregateKeys(const std::vector<bool>& peer_map);

  /// Aggregates the list of received commits.
  CommitPoint AggregateCommits(const std::vector<CommitPoint>& commits);

  /// Aggregates the list of received responses.
  Response AggregateResponses(const std::vector<Response>& responses);

  /// Generates the collective signature.
  Signature AggregateSign(const Challenge& challenge,
                          const Response& aggregated_response);

  /// Generates the challenge according to the aggregated commit and key.
  Challenge GetChallenge(const bytes& msg, const CommitPoint& aggregated_commit,
                         const PubKey& aggregated_key);

  PairOfNode GetCommitteeMember(const unsigned int index);

 public:
  /// Consensus message processing function
  virtual bool ProcessMessage([[gnu::unused]] const bytes& message,
                              [[gnu::unused]] unsigned int offset,
                              [[gnu::unused]] const Peer& from) {
    return false;  // Should be implemented by ConsensusLeader and
                   // ConsensusBackup
  }

  /// The minimum fraction of peers necessary to achieve consensus.
  static constexpr double TOLERANCE_FRACTION = 0.667;

  /// Returns the state of the active consensus session
  State GetState() const;

  /// Returns some general data about the consensus message
  bool PreProcessMessage(const bytes& message, const unsigned int offset,
                         uint32_t& consensusID, PubKey& senderPubKey,
                         bytes& reserializedMessage) const;

  /// Returns the consensus error code
  ConsensusErrorCode GetConsensusErrorCode() const;

  /// Returns the consensus error message
  std::string GetConsensusErrorMsg() const;

  /// Set consensus error code
  void SetConsensusErrorCode(ConsensusErrorCode ErrorCode);

  /// For recovery. Roll back to a certain state
  void RecoveryAndProcessFromANewState(State newState);

  /// Returns the co-sig for first round
  const Signature& GetCS1() const;

  /// Returns the co-sig bitmap for first round
  const std::vector<bool>& GetB1() const;

  /// Returns the co-sig for second round
  const Signature& GetCS2() const;

  /// Returns the co-sig bitmap for second round
  const std::vector<bool>& GetB2() const;

  /// Returns the fraction of the shard required to achieve consensus
  static unsigned int NumForConsensus(unsigned int shardSize);

  /// Checks whether the message can be processed now
  bool CanProcessMessage(const bytes& message, unsigned int offset);

  /// Returns a string representation of the current state
  std::string GetStateString() const;

  virtual unsigned int GetNumForConsensusFailure() = 0;

  /// Return a string respresentation of the given state
  std::string GetStateString(const State state) const;

 private:
  static std::map<State, std::string> ConsensusStateStrings;
};

#endif  // __CONSENSUSCOMMON_H__
