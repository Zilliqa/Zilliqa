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

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <map>
#include <memory>
#include <ostream>
#include <vector>

namespace RRS {

class Message {
 public:
  // ENUMS
  enum class Type {
    UNDEFINED = 0x00,
    PUSH = 0x01,
    PULL = 0x02,
    EMPTY_PUSH = 0x03,
    EMPTY_PULL = 0x04,
    FORWARD = 0x05,
    LAZY_PUSH = 0x06,
    LAZY_PULL = 0x07,
    NUM_TYPES
  };

  static std::map<Type, std::string> s_enumKeyToString;

 private:
  // MEMBERS
  Type m_type;
  int m_rumorId;
  int m_rounds;

 public:
  // CONSTRUCTORS
  Message();

  Message(Type type, int rumorId, int rounds);

  // OPERATORS
  bool operator==(const Message& other) const;

  bool operator!=(const Message& other) const;

  friend std::ostream& operator<<(std::ostream& os, const Message& message);

  // CONST METHODS
  Type type() const;

  int rumorId() const;

  int rounds() const;
};

}  // namespace RRS

#endif  //__MESSAGE_H__
