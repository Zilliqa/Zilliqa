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

#ifndef __CONSENSUSLEADER_H__
#define __CONSENSUSLEADER_H__

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "ConsensusCommon.h"
#include "libCrypto/MultiSig.h"
#include "libUtils/TimeLockedFunction.h"

typedef std::function<bool(const bytes& errorMsg, const Peer& from)>
    NodeCommitFailureHandlerFunc;
typedef std::function<bool(std::map<unsigned int, bytes>)>
    ShardCommitFailureHandlerFunc;
typedef std::function<bool(
    bytes& dst, unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PairOfKey& leaderKey, bytes& messageToCosign)>
    AnnouncementGeneratorFunc;

/// Implements the functionality for the consensus committee leader.
class ConsensusLeader : public ConsensusCommon {
  enum Action {
    SEND_ANNOUNCEMENT = 0x00,
    PROCESS_COMMIT,
    PROCESS_RESPONSE,
    PROCESS_FINALCOMMIT,
    PROCESS_FINALRESPONSE,
    PROCESS_COMMITFAILURE
  };

  // Consensus session settings
  unsigned int m_numForConsensus;
  unsigned int m_numForConsensusFailure;

  bool m_DS;
  unsigned int m_numOfSubsets;
  // Received commits
  std::mutex m_mutex;
  std::atomic<unsigned int> m_commitCounter;

  std::mutex m_mutexAnnounceSubsetConsensus;
  std::condition_variable cv_scheduleSubsetConsensus;
  bool m_sufficientCommitsReceived;
  unsigned int m_sufficientCommitsNumForSubsets;

  std::vector<bool> m_commitMap;
  std::vector<CommitPoint>
      m_commitPointMap;  // ordered list of commits of size = committee size
  std::vector<CommitPoint> m_commitPoints;  // unordered list of commits of size
                                            // = 2/3 of committee size + 1
  unsigned int m_commitRedundantCounter;
  std::vector<bool> m_commitRedundantMap;
  std::vector<CommitPoint>
      m_commitRedundantPointMap;  // ordered list of redundant commits of size =
                                  // 1/3 of committee size
  // Generated challenge
  Challenge m_challenge;

  unsigned int m_commitFailureCounter;
  std::map<unsigned int, bytes> m_commitFailureMap;

  // Tracking data for each consensus subset
  struct ConsensusSubset {
    std::vector<bool> commitMap;
    std::vector<CommitPoint> commitPointMap;  // Ordered list of commits of
                                              // fixed size = committee size
    std::vector<CommitPoint> commitPoints;
    unsigned int responseCounter;
    Challenge challenge;  // Challenge / Finalchallenge value generated
    std::vector<Response> responseDataMap;  // Ordered list of responses of
                                            // fixed size = committee size
    /// Response map for the generated collective signature
    std::vector<bool> responseMap;
    std::vector<Response> responseData;
    Signature collectiveSig;
    State state;  // Subset consensus state
  };
  std::vector<ConsensusSubset> m_consensusSubsets;
  unsigned int m_numSubsetsRunning;

  NodeCommitFailureHandlerFunc m_nodeCommitFailureHandlerFunc;
  ShardCommitFailureHandlerFunc m_shardCommitFailureHandlerFunc;

  // Internal functions
  bool CheckState(Action action);
  bool CheckStateSubset(uint16_t subsetID, Action action);
  void SetStateSubset(uint16_t subsetID, State newState);
  void GenerateConsensusSubsets();
  bool StartConsensusSubsets();
  void SubsetEnded(uint16_t subsetID);
  bool ProcessMessageCommitCore(const bytes& commit, unsigned int offset,
                                Action action,
                                ConsensusMessageType returnmsgtype,
                                State nextstate, const Peer& from);
  bool ProcessMessageCommit(const bytes& commit, unsigned int offset,
                            const Peer& from);
  bool ProcessMessageCommitFailure(const bytes& commitFailureMsg,
                                   unsigned int offset, const Peer& from);
  bool GenerateChallengeMessage(bytes& challenge, unsigned int offset);
  bool ProcessMessageResponseCore(const bytes& response, unsigned int offset,
                                  Action action,
                                  ConsensusMessageType returnmsgtype,
                                  State nextstate, const Peer& from);
  bool ProcessMessageResponse(const bytes& response, unsigned int offset,
                              const Peer& from);
  bool GenerateCollectiveSigMessage(bytes& collectivesig, unsigned int offset,
                                    uint16_t subsetID);
  bool ProcessMessageFinalCommit(const bytes& finalcommit, unsigned int offset,
                                 const Peer& from);
  bool ProcessMessageFinalResponse(const bytes& finalresponse,
                                   unsigned int offset, const Peer& from);

 public:
  /// Constructor.
  ConsensusLeader(
      uint32_t consensus_id,    // unique identifier for this consensus session
      uint64_t block_number,    // latest final block number
      const bytes& block_hash,  // unique identifier for this consensus session
      uint16_t node_id,  // leader's identifier (= index in some ordered lookup
                         // table shared by all nodes)
      const PrivKey& privkey,        // leader's private key
      const DequeOfNode& committee,  // ordered lookup table of pubkeys for this
                                     // committee (includes leader)
      unsigned char class_byte,      // class byte representing Executable class
                                     // using this instance of ConsensusLeader
      unsigned char ins_byte,        // instruction byte representing consensus
                                     // messages for the Executable class
      NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
      ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc,
      bool isDS = false);
  /// Destructor.
  ~ConsensusLeader();

  /// Triggers the start of consensus on a particular message (e.g., DS block).

  bool StartConsensus(AnnouncementGeneratorFunc announcementGeneratorFunc,
                      bool useGossipProto = false);

  /// Function to process any consensus message received.
  bool ProcessMessage(const bytes& message, unsigned int offset,
                      const Peer& from);

  unsigned int GetNumForConsensusFailure() { return m_numForConsensusFailure; }

  /// Function to check for missing responses
  void Audit();

  /// Function to log the responses stats
  void LogResponsesStats(unsigned int subsetID);

 private:
  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
};

#endif  // __CONSENSUSLEADER_H__
