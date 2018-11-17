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
