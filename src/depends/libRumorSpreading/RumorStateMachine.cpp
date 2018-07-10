#include "RumorStateMachine.h"

#define LITERAL(s) #s

namespace RRS {

// STATIC MEMBERS
std::map<RumorStateMachine::State, std::string> RumorStateMachine::s_enumKeyToString = {
    {UNKNOWN,    LITERAL(UNKNOWN)},
    {NEW,        LITERAL(NEW)},
    {KNOWN,      LITERAL(KNOWN)},
    {OLD,        LITERAL(OLD)},
    {NUM_STATES, LITERAL(NUM_STATES)}
};


// PRIVATE METHODS
void RumorStateMachine::advanceNew(const std::unordered_set<int>& membersInRound)
{
    m_roundsInB++;
    if (m_age >= m_networkConfigPtr->maxRoundsTotal()) {
        advanceOld();
        return;
    }

    for (auto id : membersInRound) {
        if (m_memberRounds.count(id) <= 0) m_memberRounds[id] = 0;
    }

    // Compare our age to the majority of rounds
    int numLess = 0;
    int numGreaterOrEqual = 0;
    for (const auto entry : m_memberRounds) {
        int theirRound = entry.second;
        if (theirRound < m_age) {
            numLess++;
        } else if (theirRound >= m_networkConfigPtr->maxRoundsInB()) {
            m_state = KNOWN;
        } else {
            numGreaterOrEqual++;
        }
    }

    if (numGreaterOrEqual > numLess) {
        m_roundsInB++;
    }

    if (m_roundsInB >= m_networkConfigPtr->maxRoundsInB()) {
        m_state = KNOWN;
    }
    m_memberRounds.clear();
}

void RumorStateMachine::advanceKnown()
{
    m_roundsInC++;
    if (m_age >= m_networkConfigPtr->maxRoundsTotal() ||
        m_roundsInC >= m_networkConfigPtr->maxRoundsInC()) {
        advanceOld();
    }

}

void RumorStateMachine::advanceOld()
{
    m_state = OLD;
    m_memberRounds.clear();
}

// CONSTRUCTORS
RumorStateMachine::RumorStateMachine()
: m_state(State::UNKNOWN)
  , m_networkConfigPtr(nullptr)
  , m_age(-1)
  , m_roundsInB(-1)
  , m_roundsInC(-1)
  , m_memberRounds()
{
}

RumorStateMachine::RumorStateMachine(const NetworkConfig* networkConfigPtr)
: m_state(State::NEW)
  , m_networkConfigPtr(networkConfigPtr)
  , m_age(0)
  , m_roundsInB(0)
  , m_roundsInC(0)
  , m_memberRounds()
{
}

RumorStateMachine::RumorStateMachine(const NetworkConfig* networkConfigPtr,
                                     int fromMember,
                                     int theirRound)
: m_state(State::NEW)
  , m_networkConfigPtr(networkConfigPtr)
  , m_age(0)
  , m_roundsInB(0)
  , m_roundsInC(0)
  , m_memberRounds()
{
    // Maximum number of rounds reached
    if (theirRound > m_networkConfigPtr->maxRoundsTotal()) {
        advanceOld();
        return;
    }

    // Stay in B-m state
    m_memberRounds[fromMember] = theirRound;
}

void RumorStateMachine::rumorReceived(int memberId, int theirRound)
{
    // Only care about other members when the rumor is NEW
    if (m_state == NEW) {
        if (m_memberRounds.count(memberId) > 0) {
            throw std::logic_error("Received a message from the same member within a single round");
        }
        m_memberRounds[memberId] = theirRound;
    }
}

void RumorStateMachine::advanceRound(const std::unordered_set<int>& peersInCurrentRound)
{
    m_age++;
    switch (m_state) {
        case NEW:
            advanceNew(peersInCurrentRound);
            return;
        case KNOWN:
            advanceKnown();
            return;
        case OLD:
            m_age++;
            return;
        case UNKNOWN:
        default:
            throw std::logic_error("Unexpected state: " + s_enumKeyToString[m_state]);
    }
}

const RumorStateMachine::State RumorStateMachine::state() const
{
    return m_state;
}

const int RumorStateMachine::age() const
{
    return m_age;
}

const bool RumorStateMachine::isOld() const
{
    return m_state == OLD;
}

std::ostream& operator<<(std::ostream& os, const RumorStateMachine& machine)
{
    os << "{ state: " << RumorStateMachine::s_enumKeyToString[machine.m_state]
       << ", currentRound: " << machine.m_age
       << ", roundsInB: " << machine.m_roundsInB
       << ", roundsInC: " << machine.m_roundsInC
       << "}";
    return os;
}

} // project namespace