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
