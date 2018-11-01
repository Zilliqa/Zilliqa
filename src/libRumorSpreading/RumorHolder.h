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
