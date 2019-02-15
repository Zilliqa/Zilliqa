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

#ifndef __CONSENSUSBACKUP_H__
#define __CONSENSUSBACKUP_H__

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "ConsensusCommon.h"
#include "libCrypto/MultiSig.h"
#include "libUtils/TimeLockedFunction.h"

typedef std::function<bool(const bytes& input, unsigned int offset,
                           bytes& errorMsg, const uint32_t consensusID,
                           const uint64_t blockNumber, const bytes& blockHash,
                           const uint16_t leaderID, const PubKey& leaderKey,
                           bytes& messageToCosign)>
    MsgContentValidatorFunc;

/// Implements the functionality for the consensus committee backup.
class ConsensusBackup : public ConsensusCommon {
 private:
  enum Action {
    PROCESS_ANNOUNCE = 0x00,
    PROCESS_CHALLENGE,
    PROCESS_COLLECTIVESIG,
    PROCESS_FINALCHALLENGE,
    PROCESS_FINALCOLLECTIVESIG
  };

  // Consensus session settings
  uint16_t m_leaderID;

  // Function handler for validating message content
  MsgContentValidatorFunc m_msgContentValidator;

  // Internal functions
  bool CheckState(Action action);

  bool ProcessMessageAnnounce(const bytes& announcement, unsigned int offset);
  bool GenerateCommitFailureMessage(bytes& commitFailure, unsigned int offset,
                                    const bytes& errorMsg);
  bool ProcessMessageConsensusFailure([[gnu::unused]] const bytes& announcement,
                                      [[gnu::unused]] unsigned int offset);
  bool GenerateCommitMessage(bytes& commit, unsigned int offset);
  bool ProcessMessageChallengeCore(const bytes& challenge, unsigned int offset,
                                   Action action,
                                   ConsensusMessageType returnmsgtype,
                                   State nextstate);
  bool ProcessMessageChallenge(const bytes& challenge, unsigned int offset);
  bool GenerateResponseMessage(
      bytes& response, unsigned int offset,
      const std::vector<ResponseSubsetInfo>& subsetInfo);
  bool ProcessMessageCollectiveSigCore(const bytes& collectivesig,
                                       unsigned int offset, Action action,
                                       State nextstate);
  bool ProcessMessageCollectiveSig(const bytes& collectivesig,
                                   unsigned int offset);
  bool ProcessMessageFinalChallenge(const bytes& challenge,
                                    unsigned int offset);
  bool ProcessMessageFinalCollectiveSig(const bytes& finalcollectivesig,
                                        unsigned int offset);

 public:
  /// Constructor.
  ConsensusBackup(
      uint32_t consensus_id,    // unique identifier for this consensus session
      uint64_t block_number,    // latest final block number
      const bytes& block_hash,  // unique identifier for this consensus session
      uint16_t node_id,  // backup's identifier (= index in some ordered lookup
                         // table shared by all nodes)
      uint16_t leader_id,      // leader's identifier (= index in some ordered
                               // lookup table shared by all nodes)
      const PrivKey& privkey,  // backup's private key
      const DequeOfNode& committee,  // ordered lookup table of pubkeys for this
                                     // committee (includes leader)
      uint8_t class_byte,            // class byte representing Executable class
                                     // using this instance of ConsensusBackup
      uint8_t ins_byte,              // instruction byte representing consensus
                                     // messages for the Executable class
      MsgContentValidatorFunc
          msg_validator  // function handler for validating the content of
                         // message for consensus (e.g., Tx block)
  );

  /// Destructor.
  ~ConsensusBackup();

  /// Function to process any consensus message received.
  bool ProcessMessage(const bytes& message, unsigned int offset,
                      const Peer& from);

  unsigned int GetNumForConsensusFailure() { return 0; }

 private:
  static std::map<Action, std::string> ActionStrings;
  std::string GetActionString(Action action) const;
};

#endif  // __CONSENSUSBACKUP_H__
