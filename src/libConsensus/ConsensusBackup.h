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
#include "libNetwork/PeerStore.h"
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

  // Received challenge
  Challenge m_challenge;

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
  bool GenerateResponseMessage(bytes& response, unsigned int offset,
                               uint16_t subsetID);
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
      const std::deque<std::pair<PubKey, Peer>>&
          committee,       // ordered lookup table of pubkeys for this committee
                           // (includes leader)
      uint8_t class_byte,  // class byte representing Executable class
                           // using this instance of ConsensusBackup
      uint8_t ins_byte,    // instruction byte representing consensus
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
