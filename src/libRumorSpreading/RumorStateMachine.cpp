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

#include "RumorStateMachine.h"

#define LITERAL(s) #s

namespace RRS {

// STATIC MEMBERS
std::map<RumorStateMachine::State, std::string>
    RumorStateMachine::s_enumKeyToString = {
        {State::UNKNOWN, LITERAL(UNKNOWN)},
        {State::NEW, LITERAL(NEW)},
        {State::KNOWN, LITERAL(KNOWN)},
        {State::OLD, LITERAL(OLD)},
        {State::NUM_STATES, LITERAL(NUM_STATES)}};

// PRIVATE METHODS
void RumorStateMachine::advanceFromNew(
    const std::unordered_set<int>& membersInRound) {
  ++m_roundsInB;
  if (m_rounds > m_networkConfigPtr->maxRoundsTotal()) {
    advanceToOld();
    return;
  }

  for (auto id : membersInRound) {
    if (m_memberRounds.count(id) <= 0) {
      m_memberRounds[id] = 0;
    }
  }

  // Compare our round to the majority of rounds
  int numLess = 0;
  int numGreaterOrEqual = 0;
  for (const auto& entry : m_memberRounds) {
    int theirRound = entry.second;
    if (theirRound < m_rounds) {
      ++numLess;
    } else if (theirRound > m_networkConfigPtr->maxRoundsInB()) {
      m_state = State::KNOWN;
    } else {
      ++numGreaterOrEqual;
    }
  }

  if (numGreaterOrEqual > numLess) {
    ++m_roundsInB;
  }

  if (m_roundsInB > m_networkConfigPtr->maxRoundsInB()) {
    m_state = State::KNOWN;
  }
  m_memberRounds.clear();
}

void RumorStateMachine::advanceFromKnown() {
  ++m_roundsInC;
  if (m_rounds > m_networkConfigPtr->maxRoundsTotal() ||
      m_roundsInC > m_networkConfigPtr->maxRoundsInC()) {
    advanceToOld();
  }
}

void RumorStateMachine::advanceToOld() {
  m_state = State::OLD;
  m_memberRounds.clear();
}

// CONSTRUCTORS

RumorStateMachine::RumorStateMachine(const NetworkConfig* networkConfigPtr)
    : m_state(State::NEW),
      m_networkConfigPtr(networkConfigPtr),
      m_rounds(0),
      m_roundsInB(0),
      m_roundsInC(0),
      m_memberRounds() {}

RumorStateMachine::RumorStateMachine(const NetworkConfig* networkConfigPtr,
                                     int fromMember, int theirRound)
    : m_state(State::NEW),
      m_networkConfigPtr(networkConfigPtr),
      m_rounds(0),
      m_roundsInB(0),
      m_roundsInC(0),
      m_memberRounds() {
  // Maximum number of rounds reached
  if (theirRound > m_networkConfigPtr->maxRoundsTotal()) {
    advanceToOld();
    return;
  }

  // Stay in B-m state
  m_memberRounds[fromMember] = theirRound;
}

void RumorStateMachine::rumorReceived(int memberId, int theirRound) {
  // Only care about other members when the rumor is NEW
  if (m_state == State::NEW) {
    if (m_memberRounds[memberId] < theirRound) {
      m_memberRounds[memberId] = theirRound;
    }
  }
}

void RumorStateMachine::advanceRound(
    const std::unordered_set<int>& peersInCurrentRound) {
  ++m_rounds;
  switch (m_state) {
    case State::NEW:
      advanceFromNew(peersInCurrentRound);
      return;
    case State::KNOWN:
      advanceFromKnown();
      return;
    case State::OLD:
      ++m_rounds;
      return;
    case State::UNKNOWN:
    default:
      throw std::logic_error("Unexpected state: " + s_enumKeyToString[m_state]);
  }
}

RumorStateMachine::State RumorStateMachine::state() const { return m_state; }

int RumorStateMachine::rounds() const { return m_rounds; }

bool RumorStateMachine::isOld() const { return m_state == State::OLD; }

std::ostream& operator<<(std::ostream& os, const RumorStateMachine& machine) {
  os << "{ state: " << RumorStateMachine::s_enumKeyToString[machine.m_state]
     << ", currentRound: " << machine.m_rounds
     << ", roundsInB: " << machine.m_roundsInB
     << ", roundsInC: " << machine.m_roundsInC << "}";
  return os;
}

}  // namespace RRS