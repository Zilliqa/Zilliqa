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

#ifndef __RUMORSTATEMACHINE_H__
#define __RUMORSTATEMACHINE_H__

#include <array>
#include <functional>
#include <map>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include "NetworkConfig.h"

namespace RRS {

class RumorStateMachine {
 public:
  // ENUMS
  enum class State {
    UNKNOWN,  // initial state where the peer 'v' doesn't know about the rumor
              // 'r'
    NEW,      // the peer 'v' knows 'r' and counter(v,r) = m
    KNOWN,    // cooling state, stay in this state for a 'm_maxRounds' rounds
    OLD,      // final state, member stops participating in rumor spreading
    NUM_STATES
  };

  static std::map<State, std::string> s_enumKeyToString;

 private:
  // MEMBERS
  State m_state;
  const NetworkConfig* m_networkConfigPtr;
  int m_rounds;
  int m_roundsInB;
  int m_roundsInC;
  std::unordered_map<int, int> m_memberRounds;  // Member ID --> rounds

  // METHODS
  void advanceFromNew(const std::unordered_set<int>& membersInRound);

  void advanceFromKnown();

  void advanceToOld();

 public:
  // CONSTRUCTORS
  // Default constructor. The returned state machine instance will be in an
  // invalid state.
  RumorStateMachine() = delete;

  // Construct a new instance using the specified 'networkConfigPtr'.
  RumorStateMachine(const NetworkConfig* networkConfigPtr);

  // Construct a new instance using the specified 'networkConfigPtr',
  // 'fromMember' and 'theirRound' parameters.
  RumorStateMachine(const NetworkConfig* networkConfigPtr, int fromMember,
                    int theirRound);

  // Copy constructor.
  RumorStateMachine(const RumorStateMachine& other) = default;

  // Move constructor.
  RumorStateMachine(RumorStateMachine&& other) = default;

  // OPERATORS
  // Assignment operator.
  RumorStateMachine& operator=(const RumorStateMachine& other) = default;

  // Move assignment operator.
  RumorStateMachine& operator=(RumorStateMachine&& other) = default;

  // METHODS
  void rumorReceived(int memberId, int theirRound);

  void advanceRound(const std::unordered_set<int>& peersInCurrentRound);

  // CONST METHODS
  State state() const;

  int rounds() const;

  bool isOld() const;

  friend std::ostream& operator<<(std::ostream& os,
                                  const RumorStateMachine& machine);
};

}  // namespace RRS

#endif  //__RUMORSTATEMACHINE_H__
