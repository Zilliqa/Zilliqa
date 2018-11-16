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

#include "Message.h"

#define LITERAL(s) #s

namespace RRS {

// STATIC MEMBERS
std::map<Message::Type, std::string> Message::s_enumKeyToString = {
    {Type::UNDEFINED, LITERAL(UNDEFINED)},
    {Type::LAZY_PUSH, LITERAL(LAZY_PUSH)},
    {Type::LAZY_PULL, LITERAL(LAZY_PULL)},
    {Type::PUSH, LITERAL(PUSH)},
    {Type::PULL, LITERAL(PULL)},
    {Type::EMPTY_PUSH, LITERAL(EMPTY_PUSH)},
    {Type::EMPTY_PULL, LITERAL(EMPTY_PULL)},
    {Type::FORWARD, LITERAL(FORWARD)}};

// CONSTRUCTORS
Message::Message() {}

Message::Message(Message::Type type, int rumorId, int rounds)
    : m_type(type), m_rumorId(rumorId), m_rounds(rounds) {}

// OPERATORS
bool Message::operator==(const Message& other) const {
  return m_type == other.m_type && m_rumorId == other.m_rumorId &&
         m_rounds == other.m_rounds;
}

bool Message::operator!=(const Message& other) const {
  return !(*this == other);
}

// CONST METHODS
Message::Type Message::type() const { return m_type; }

int Message::rumorId() const { return m_rumorId; }

int Message::rounds() const { return m_rounds; }

// FREE OPERATORS
std::ostream& operator<<(std::ostream& os, const Message& message) {
  os << "[ type: " << Message::s_enumKeyToString[message.m_type]
     << " rumorId: " << message.m_rumorId << " Rounds: " << message.m_rounds
     << "]";
  return os;
}

}  // namespace RRS