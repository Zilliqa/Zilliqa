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
    // correct the actual total rounds spent over-all before switching to OLD
    --m_rounds;
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

  if (m_state == State::KNOWN) {
    // by now , rumor already moved to C-State
    ++m_roundsInC;
    // correct the actual rounds spent in B-State
    --m_roundsInB;
  }

  m_memberRounds.clear();
}

void RumorStateMachine::advanceFromKnown() {
  ++m_roundsInC;
  m_state = State::KNOWN;
  if (m_rounds > m_networkConfigPtr->maxRoundsTotal() ||
      m_roundsInC > m_networkConfigPtr->maxRoundsInC()) {
    // correct the actual rounds spent in C-State
    --m_roundsInC;
    // correct the actual total rounds spent over-all before switching to OLD
    --m_rounds;
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
  } else if (theirRound > m_networkConfigPtr->maxRoundsInB()) {
    advanceFromKnown();  // move directly to C-State
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
     << ", round: " << machine.m_rounds << ", roundsB: " << machine.m_roundsInB
     << ", roundsC: " << machine.m_roundsInC << "}";
  return os;
}

}  // namespace RRS
