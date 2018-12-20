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

#ifndef __BLACKLIST_H__
#define __BLACKLIST_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <mutex>
#include <unordered_map>

namespace std {
template <>
struct hash<boost::multiprecision::uint128_t> {
  std::size_t operator()(const boost::multiprecision::uint128_t& key) const {
    return std::hash<std::string>()(key.convert_to<std::string>());
  }
};
}  // namespace std

class Blacklist {
  Blacklist();
  ~Blacklist();

  // Singleton should not implement these
  Blacklist(Blacklist const&) = delete;
  void operator=(Blacklist const&) = delete;

  std::mutex m_mutexBlacklistIP;
  std::unordered_map<boost::multiprecision::uint128_t, bool> m_blacklistIP;

 public:
  static Blacklist& GetInstance();

  /// P2PComm may use this function
  bool Exist(const boost::multiprecision::uint128_t& ip);

  /// P2PComm may use this function to blacklist certain non responding nodes
  void Add(const boost::multiprecision::uint128_t& ip);

  /// P2PComm may use this function to remove a node form blacklist
  void Remove(const boost::multiprecision::uint128_t& ip);

  /// Node can clear the blacklist
  void Clear();
};

#endif  // __BLACKLIST_H__
