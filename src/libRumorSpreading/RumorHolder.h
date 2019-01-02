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

#ifndef __RUMORHOLDER_H__
#define __RUMORHOLDER_H__

#include <functional>
#include <map>
#include <mutex>
#include <unordered_set>

#include "MemberID.h"
#include "NetworkConfig.h"
#include "RumorSpreadingInterface.h"
#include "RumorStateMachine.h"

namespace RRS {

// This is a thread-safe implementation of the 'RumorSpreadingInterface'.
class RumorHolder : public RumorSpreadingInterface {
 public:
  // TYPES
  // typedef std::function<int()> NextMemberCb;
  using NextMemberCb = std::function<int()>;

  // ENUMS
  enum class StatisticKey {
    NumPeers,
    NumMessagesReceived,
    Rounds,
    NumLazyPushMessages,
    NumEmptyPushMessages,
    NumLazyPullMessages,
    NumEmptyPullMessages,
  };

  static std::map<StatisticKey, std::string> s_enumKeyToString;

 private:
  // MEMBERS
  const int m_id;
  NetworkConfig m_networkConfig;
  std::vector<int> m_peers;
  std::unordered_set<int> m_peersInCurrentRound;
  std::unordered_map<int, RumorStateMachine> m_rumors;
  mutable std::mutex m_mutex;
  NextMemberCb m_nextMemberCb;
  std::unordered_set<int> m_nonPriorityPeers;
  std::map<StatisticKey, double> m_statistics;
  int m_maxNeighborsPerRound;

  static const int MAX_RETRY = 3;

  // METHODS
  // Copy the member ids into a vector
  void toVector(const std::unordered_set<int>& peers);

  // Return a randomly selected member id
  int chooseRandomMember();

  // Add the specified 'value' to the previous statistic value
  void increaseStatValue(StatisticKey key, double value);

 public:
  // CONSTRUCTORS
  /// Create an instance which automatically figures out the network parameters.
  RumorHolder(const std::unordered_set<int>& peers, int id = MemberID::next());
  RumorHolder(const std::unordered_set<int>& peers, const NextMemberCb& cb,
              int id = MemberID::next());

  /// Used for manually passed network parameters.
  RumorHolder(const std::unordered_set<int>& peers,
              const NetworkConfig& networkConfig, int id = MemberID::next());
  RumorHolder(const std::unordered_set<int>& peers,
              const NetworkConfig& networkConfig, const NextMemberCb& cb,
              int id = MemberID::next());
  RumorHolder(const std::unordered_set<int>& peers, int maxRoundsInB,
              int maxRoundsInC, int maxTotalRounds, int maxNeighborsPerRound,
              int id);

  RumorHolder(const RumorHolder& other);

  RumorHolder(RumorHolder&& other) noexcept;

  // METHODS
  bool addRumor(int rumorId) override;

  std::pair<int, std::vector<Message>> receivedMessage(const Message& message,
                                                       int fromPeer) override;

  std::pair<std::vector<int>, std::vector<Message>> advanceRound() override;

  // CONST METHODS
  int id() const;

  const NetworkConfig& networkConfig() const;

  const std::unordered_map<int, RumorStateMachine>& rumorsMap() const;

  bool rumorExists(int rumorId) const;

  bool isOld(int rumorId) const;

  const std::map<StatisticKey, double>& statistics() const;

  std::ostream& printStatistics(std::ostream& outStream) const;

  bool operator==(const RumorHolder& other) const;
};

}  // namespace RRS

#endif  //__RUMORHOLDER_H__
