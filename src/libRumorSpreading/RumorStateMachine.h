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
