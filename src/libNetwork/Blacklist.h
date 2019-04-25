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

#ifndef __BLACKLIST_H__
#define __BLACKLIST_H__

#include <atomic>
#include <mutex>
#include <set>
#include <unordered_map>

#include "common/BaseType.h"

namespace std {
template <>
struct hash<uint128_t> {
  std::size_t operator()(const uint128_t& key) const {
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
  std::unordered_map<uint128_t, bool> m_blacklistIP;
  std::set<uint128_t> m_excludedIP;
  std::atomic<bool> m_enabled;

 public:
  static Blacklist& GetInstance();

  /// P2PComm may use this function
  bool Exist(const uint128_t& ip);

  /// P2PComm may use this function to blacklist certain non responding nodes
  void Add(const uint128_t& ip);

  /// P2PComm may use this function to remove a node form blacklist
  void Remove(const uint128_t& ip);

  /// Node can clear the blacklist
  void Clear();

  /// Remove n nodes from blacklist
  void Pop(unsigned int num_to_pop);

  /// Remove n nodes from blacklist
  unsigned int SizeOfBlacklist();

  /// Enable / disable blacklist
  void Enable(const bool enable);

  /// Node to be excluded from blacklisting
  bool Exclude(const uint128_t& ip);

  /// Remove node from exclusion list for blacklisting
  bool RemoveExclude(const uint128_t& ip);
};

#endif  // __BLACKLIST_H__
