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

#ifndef __GUARD_H__
#define __GUARD_H__

#include <mutex>
#include <unordered_map>

#include "Peer.h"
#include "libCrypto/Schnorr.h"
#include "libMediator/Mediator.h"

class Guard {
  Guard();
  ~Guard();

  // Singleton should not implement these
  Guard(Guard const&) = delete;
  void operator=(Guard const&) = delete;

  // DS guardlist
  std::mutex m_mutexDSGuardList;
  std::unordered_set<PubKey> m_DSGuardList;

  // Shard guardlist
  std::mutex m_mutexShardGuardList;
  std::unordered_set<PubKey> m_ShardGuardList;

  // IPFilter
  std::mutex m_mutexIPExclusion;
  std::vector<std::pair<uint128_t, uint128_t>> m_IPExclusionRange;

  void ValidateRunTimeEnvironment();

 public:
  /// Returns the singleton Guard instance.
  static Guard& GetInstance();
  void UpdateDSGuardlist();
  void UpdateShardGuardlist();

  void AddToDSGuardlist(const PubKey& dsGuardPubKey);
  void AddToShardGuardlist(const PubKey& shardGuardPubKey);

  bool IsNodeInDSGuardList(const PubKey& nodePubKey);
  bool IsNodeInShardGuardList(const PubKey& nodePubKey);

  unsigned int GetNumOfDSGuard();
  unsigned int GetNumOfShardGuard();
  void AddDSGuardToBlacklistExcludeList(const DequeOfNode& dsComm);

  // To check if IP is a valid v4 IP and not belongs to exclusion list
  bool IsValidIP(const uint128_t& ip_addr);

  // To add limits to the exclusion list
  void AddToExclusionList(const uint128_t& ft, const uint128_t& sd);
  void AddToExclusionList(const std::string& ft, const std::string& sd);
  // Intialize
  void Init();
};

#endif  // __GUARD_H__
