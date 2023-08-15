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

#ifndef ZILLIQA_SRC_LIBNETWORK_BLACKLIST_H_
#define ZILLIQA_SRC_LIBNETWORK_BLACKLIST_H_

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

struct NodeKey {
        uint128_t ip;
        uint32_t  port;
        std::string node_id;

        // Custom equality operator for NodeKey objects
        bool operator==(const NodeKey& other) const {
            return (ip == other.ip && port == other.port && node_id == other.node_id);
        }
        bool operator<(const NodeKey& other) const {
            return (ip < other.ip || (ip == other.ip && port < other.port) || (ip == other.ip && port == other.port && node_id < other.node_id));
        }
  };

  // Custom hash function for NodeKey objects
  struct NodeKeyHash {
        std::size_t operator()(const NodeKey& key) const {
            std::size_t hash1 = std::hash<std::string>{}(key.ip.convert_to<std::string>());
            std::size_t hash2 = std::hash<int>{}(key.port);
            return hash1 ^ (hash2 << 1);
        }
  };


class Blacklist {
  Blacklist();
  ~Blacklist();

  // Singleton should not implement these
  Blacklist(Blacklist const&) = delete;
  void operator=(Blacklist const&) = delete;

  std::mutex m_mutexBlacklistIP;
  std::unordered_map<NodeKey, bool, NodeKeyHash> m_BlackListNode;
  // IP/port/node     <-> Strict/Relaxed
                      // Strict -> Blacklisted for both sending and incoming msg
                      // Relaxed -> Blacklisted for incoming msg only
  std::set<NodeKey> m_whiteListNode;
  std::mutex m_mutexWhitelistedSeedsIP;
  std::set<NodeKey> m_whitelistedSeedsNodes;
  std::atomic<bool> m_enabled;

 public:
  static Blacklist& GetInstance();

  /// P2PComm may use this function - whether exists in m_strictBlacklistIP or
  /// m_relaxedBlacklistIP
  bool Exist(const NodeKey& ip, const bool strict = true);

  /// P2PComm may use this function to blacklist certain non responding nodes
  void Add(const NodeKey& key, const bool strict = true,
           const bool ignoreWhitelist = false);



  /// P2PComm may use this function to remove a node form blacklist
  void Remove(const NodeKey& key);


  /// Node can clear the blacklist
  void Clear();

  /// Remove n nodes from blacklist
  void Pop(unsigned int num_to_pop);

  /// Remove n nodes from blacklist
  unsigned int SizeOfBlacklist();

  /// Enable / disable blacklist
  void Enable(const bool enable);

  // Check if Blacklisting/Whitelisting is enabled
  bool IsEnabled();

  /// Node to be whitelisted
  bool Whitelist(const NodeKey& ip);

  /// Remove node from whitelist
  bool RemoveFromWhitelist(const NodeKey& ip);

  /// Seeds node to be whitelisted
  bool WhitelistSeed(const NodeKey& ip);

  /// Remove node from whitelisted seeds
  bool RemoveFromWhitelistedSeeds(const NodeKey& ip);

  /// Check if given IP is a part of whitelisted ip
  bool IsWhitelistedIP(const NodeKey& ip);

  /// Special case - Whitelisted seeds - exchange seeds, level2lookups, lookups
  bool IsWhitelistedSeed(const NodeKey& ip);
};

#endif  // ZILLIQA_SRC_LIBNETWORK_BLACKLIST_H_
