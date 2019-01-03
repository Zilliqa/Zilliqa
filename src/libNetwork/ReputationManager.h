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

#ifndef __REPUTATION_MANAGER_H__
#define __REPUTATION_MANAGER_H__

#include "Peer.h"
#include "common/Constants.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class ReputationManager {
  // Custom hasher
  // Ref:
  // https://stackoverflow.com/questions/32082786/why-i-cannot-use-neither-stdunordered-map-nor-boostunordered-map-with-boost
  template <typename T>
  struct hash_str {
    size_t operator()(const T& t) const {
      return std::hash<std::string>()(t.str());
    }
  };

  ReputationManager();
  ~ReputationManager();

  // Singleton should not implement these
  ReputationManager(ReputationManager const&) = delete;
  void operator=(ReputationManager const&) = delete;

 public:
  /// Returns the singleton P2PComm instance.
  static ReputationManager& GetInstance();
  void AddNodeIfNotKnown(const boost::multiprecision::uint128_t& IPAddress);
  bool IsNodeBanned(const boost::multiprecision::uint128_t& IPAddress);
  void PunishNode(const boost::multiprecision::uint128_t& IPAddress,
                  const int32_t Penalty);
  void AwardAllNodes();
  int32_t GetReputation(const boost::multiprecision::uint128_t& IPAddress);
  void Clear();

  // To be use once hooked into core protocol
  enum PenaltyType : int32_t {
    PENALTY_CONN_REFUSE = -5,
    PENALTY_INVALID_MESSAGE = -50
  };

  enum ScoreType : int32_t {
    UPPERREPTHRESHOLD = 500,
    REPTHRESHOLD = -500,
    GOOD = 0,
    BAN_MULTIPLIER = 24,
    AWARD_FOR_GOOD_NODES = 50
  };

  std::mutex m_mutexReputations;

 private:
  std::unordered_map<boost::multiprecision::uint128_t, int32_t,
                     hash_str<boost::multiprecision::uint128_t>>
      m_Reputations;

  void AddNodeIfNotKnownInternal(
      const boost::multiprecision::uint128_t& IPAddress);
  void SetReputation(const boost::multiprecision::uint128_t& IPAddress,
                     const int32_t ReputationScore);
  void UpdateReputation(const boost::multiprecision::uint128_t& IPAddress,
                        const int32_t ReputationScoreDelta);
  std::vector<boost::multiprecision::uint128_t> GetAllKnownIP();
  void AwardNode(const boost::multiprecision::uint128_t& IPAddress);
};

#endif  // __REPUTATION_MANAGER_H__
