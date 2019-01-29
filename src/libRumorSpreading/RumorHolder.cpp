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

#include "RumorHolder.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"

#include <random>

#define LITERAL(s) #s

namespace RRS {

// STATIC MEMBERS
std::map<RumorHolder::StatisticKey, std::string>
    RumorHolder::s_enumKeyToString = {
        {StatisticKey::NumPeers, LITERAL(NumPeers)},
        {StatisticKey::NumMessagesReceived, LITERAL(NumMessagesReceived)},
        {StatisticKey::Rounds, LITERAL(Rounds)},
        {StatisticKey::NumLazyPushMessages, LITERAL(NumLazyPushMessages)},
        {StatisticKey::NumEmptyPushMessages, LITERAL(NumEmptyPushMessages)},
        {StatisticKey::NumLazyPullMessages, LITERAL(NumLazyPullMessages)},
        {StatisticKey::NumEmptyPullMessages, LITERAL(NumEmptyPullMessages)},
};

// PRIVATE METHODS
void RumorHolder::toVector(const std::unordered_set<int>& peers) {
  for (const int p : peers) {
    if (p != m_id) {
      m_peers.push_back(p);
    }
  }
  increaseStatValue(StatisticKey::NumPeers, peers.size() - 1);
}

int RumorHolder::chooseRandomMember() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dis(0,
                                         static_cast<int>(m_peers.size() - 1));
  return m_peers[dis(gen)];
}

void RumorHolder::increaseStatValue(StatisticKey key, double value) {
  if (m_statistics.count(key) <= 0) {
    m_statistics[key] = value;
  } else {
    m_statistics[key] += value;
  }
}

// CONSTRUCTORS
RumorHolder::RumorHolder(const std::unordered_set<int>& peers, int id)
    : m_id(id),
      m_networkConfig(peers.size()),
      m_peers(),
      m_rumors(),
      m_mutex(),
      m_nextMemberCb(),
      m_maxNeighborsPerRound(1) {
  toVector(peers);
}

RumorHolder::RumorHolder(const std::unordered_set<int>& peers,
                         const NextMemberCb& cb, int id)
    : m_id(id),
      m_networkConfig(peers.size()),
      m_peers(),
      m_rumors(),
      m_mutex(),
      m_nextMemberCb(cb),
      m_maxNeighborsPerRound(1) {
  toVector(peers);
}

RumorHolder::RumorHolder(const std::unordered_set<int>& peers,
                         const NetworkConfig& networkConfig, int id)
    : m_id(id),
      m_networkConfig(networkConfig),
      m_peers(),
      m_rumors(),
      m_mutex(),
      m_nextMemberCb(),
      m_statistics(),
      m_maxNeighborsPerRound(1) {
  if (networkConfig.networkSize() != peers.size()) {
    LOG_GENERAL(WARNING,
                "FATAL size of netoworkConfig does not match size of peers. "
                "networkConfig: "
                    << networkConfig.networkSize()
                    << " peers: " << peers.size());
  } else {
    toVector(peers);
  }
}

RumorHolder::RumorHolder(const std::unordered_set<int>& peers, int maxRoundsInB,
                         int maxRoundsInC, int maxTotalRounds,
                         int maxNeighborsPerRound, int id)
    : m_id(id),
      m_networkConfig(peers.size(), maxRoundsInB, maxRoundsInC, maxTotalRounds),
      m_peers(),
      m_rumors(),
      m_mutex(),
      m_nextMemberCb(),
      m_statistics(),
      m_maxNeighborsPerRound(maxNeighborsPerRound) {
  if (maxNeighborsPerRound > (int)peers.size()) {
    maxNeighborsPerRound = peers.size();
  }
  toVector(peers);
}

RumorHolder::RumorHolder(const std::unordered_set<int>& peers,
                         const NetworkConfig& networkConfig,
                         const NextMemberCb& cb, int id)
    : m_id(id),
      m_networkConfig(networkConfig),
      m_peers(),
      m_rumors(),
      m_mutex(),
      m_nextMemberCb(cb),
      m_statistics(),
      m_maxNeighborsPerRound(1) {
  if (networkConfig.networkSize() != peers.size()) {
    LOG_GENERAL(WARNING,
                "FATAL: size of netoworkConfig does not match size of peers. "
                "networkConfig: "
                    << networkConfig.networkSize()
                    << " peers: " << peers.size());
  } else {
    toVector(peers);
  }
}

// COPY CONSTRUCTOR
RumorHolder::RumorHolder(const RumorHolder& other)
    : m_id(other.m_id),
      m_networkConfig(other.m_networkConfig),
      m_peers(other.m_peers),
      m_peersInCurrentRound(other.m_peersInCurrentRound),
      m_rumors(other.m_rumors),
      m_mutex(),
      m_nextMemberCb(other.m_nextMemberCb),
      m_nonPriorityPeers(other.m_nonPriorityPeers),
      m_statistics(other.m_statistics),
      m_maxNeighborsPerRound(other.m_maxNeighborsPerRound) {}

// MOVE CONSTRUCTOR
RumorHolder::RumorHolder(RumorHolder&& other) noexcept
    : m_id(other.m_id),
      m_networkConfig(other.m_networkConfig),
      m_peers(std::move(other.m_peers)),
      m_peersInCurrentRound(std::move(other.m_peersInCurrentRound)),
      m_rumors(std::move(other.m_rumors)),
      m_mutex(),
      m_nextMemberCb(std::move(other.m_nextMemberCb)),
      m_nonPriorityPeers(std::move(other.m_nonPriorityPeers)),
      m_statistics(std::move(other.m_statistics)),
      m_maxNeighborsPerRound(other.m_maxNeighborsPerRound) {}

// PUBLIC METHODS
bool RumorHolder::addRumor(int rumorId) {
  std::lock_guard<std::mutex> guard(m_mutex);  // critical section
  return m_rumors.insert(std::make_pair(rumorId, &m_networkConfig)).second;
}

std::pair<int, std::vector<Message>> RumorHolder::receivedMessage(
    const Message& message, int fromPeer) {
  std::lock_guard<std::mutex> guard(m_mutex);  // critical section

  bool isNewPeer = m_peersInCurrentRound.insert(fromPeer).second;
  increaseStatValue(StatisticKey::NumMessagesReceived, 1);

  // If this is the first time 'fromPeer' sent a PUSH/EMPTY_PUSH message in this
  // round then respond with a PULL message for each rumor
  std::vector<Message> pullMessages;
  if (isNewPeer && ((message.type() == Message::Type::LAZY_PUSH &&
                     SEND_RESPONSE_FOR_LAZY_PUSH) ||
                    message.type() == Message::Type::EMPTY_PUSH)) {
    for (auto& kv : m_rumors) {
      RumorStateMachine& stateMach = kv.second;
      if (stateMach.rounds() > 0 and !stateMach.isOld()) {
        pullMessages.emplace_back(Message::Type::LAZY_PULL, kv.first,
                                  kv.second.rounds());
      }
    }

    // No PULL messages to sent i.e. no rumors received yet,
    // then send EMPTY_PULL Message indicating receiver to stop asking again.
    if (pullMessages.empty()) {
      pullMessages.emplace_back(Message(Message::Type::EMPTY_PULL, -1, 0));
      increaseStatValue(StatisticKey::NumEmptyPullMessages, 1);
    } else {
      increaseStatValue(StatisticKey::NumLazyPullMessages, pullMessages.size());
      m_nonPriorityPeers.insert(fromPeer);
    }
  }

  // An empty response from a peer that was sent a PULL
  const int receivedRumorId = message.rumorId();
  const int theirRound = message.rounds();
  if (receivedRumorId >= 0) {
    if (m_rumors.count(receivedRumorId) > 0) {
      m_rumors.at(receivedRumorId).rumorReceived(fromPeer, message.rounds());
    } else {
      m_rumors.insert(std::make_pair(
          receivedRumorId,
          RumorStateMachine(&m_networkConfig, fromPeer, theirRound)));
    }
  }

  return std::make_pair(fromPeer, pullMessages);
}

std::pair<std::vector<int>, std::vector<Message>> RumorHolder::advanceRound() {
  std::lock_guard<std::mutex> guard(m_mutex);  // critical section

  if (m_peers.size() == 0) {
    m_nonPriorityPeers.clear();
    m_peersInCurrentRound.clear();
    return std::make_pair(std::vector<int>{-1}, std::vector<Message>());
  }

  increaseStatValue(StatisticKey::Rounds, 1);

  std::vector<int> toMembers;
  int toMember;
  int retryCount = 0;
  int neighborC = 0;

  // Total_Peers - Non_Priority_Peers = Priority_Peers
  int Priority_Peers = m_peers.size() - m_nonPriorityPeers.size();
  if (Priority_Peers < m_maxNeighborsPerRound) {
    // Priority_Peers not enough, then we have to consider the
    // Non_Prioirty_Peers aswell.
    m_nonPriorityPeers.clear();
  }

  std::unordered_set<int> tmp;
  int maxRetry = (int)m_peers.size() - m_maxNeighborsPerRound;
  if (maxRetry < MAX_RETRY) {
    // we reach here when no. of peers is very close to m_maxNeighborsPerRound;
    // So lets set it to bare min. default.
    maxRetry = MAX_RETRY;
  }
  while (neighborC < m_maxNeighborsPerRound && retryCount < maxRetry) {
    toMember = m_nextMemberCb ? m_nextMemberCb() : chooseRandomMember();
    if (tmp.count(toMember) == 0  // checks if not already found
        && m_nonPriorityPeers.count(toMember) ==
               0)  // checks if it is not a part of Non-PriorityPeers.
    {
      // Great! we found one which is not in Non-Priority-List and also which is
      // not already found.
      tmp.insert(toMember);
      ++neighborC;
      retryCount = 0;                 // reset
      toMembers.push_back(toMember);  // save it!
    } else {
      // retry again
      retryCount++;
    }
  }
  // if still no enough neighbors, try to add from nonPriorPeers list
  if (neighborC < m_maxNeighborsPerRound) {
    LOG_GENERAL(DEBUG, "Got " << neighborC << " neighbors. Expected: "
                              << m_maxNeighborsPerRound);
    LOG_GENERAL(DEBUG,
                "Didn't find enough neighbors from priority peer list. "
                "Will try selecting from NonPriority peer list");
    for (const auto& i : m_nonPriorityPeers) {
      if (neighborC < m_maxNeighborsPerRound) {
        toMembers.push_back(i);
        ++neighborC;
      } else {
        break;
      }
    }
    if (neighborC == m_maxNeighborsPerRound) {
      LOG_GENERAL(DEBUG, "Finally got enough neighbors");
    } else {
      LOG_GENERAL(DEBUG,
                  "Didn't found enough neighbors. Will send gossip "
                  "to those we found.");
    }
  }
  m_nonPriorityPeers.clear();

  // Construct the push messages
  std::vector<Message> pushMessages;
  for (auto& r : m_rumors) {
    RumorStateMachine& stateMach = r.second;

    if (stateMach.isOld()) {
      continue;
    }

    stateMach.advanceRound(m_peersInCurrentRound);
    if (!stateMach.isOld()) {
      pushMessages.emplace_back(
          Message(Message::Type::LAZY_PUSH, r.first, r.second.rounds()));
    }
  }
  increaseStatValue(StatisticKey::NumLazyPushMessages, pushMessages.size());

  // No PUSH messages but still want to sent a response to peer.
  if (pushMessages.empty()) {
    pushMessages.emplace_back(Message(Message::Type::EMPTY_PUSH, -1, 0));
    increaseStatValue(StatisticKey::NumEmptyPushMessages, 1);
  }

  // Clear round state
  m_peersInCurrentRound.clear();

  return std::make_pair(toMembers, pushMessages);
}

// PUBLIC CONST METHODS
int RumorHolder::id() const { return m_id; }

const NetworkConfig& RumorHolder::networkConfig() const {
  return m_networkConfig;
}

const std::unordered_map<int, RumorStateMachine>& RumorHolder::rumorsMap()
    const {
  return m_rumors;
}

const std::map<RumorHolder::StatisticKey, double>& RumorHolder::statistics()
    const {
  return m_statistics;
}

bool RumorHolder::rumorExists(int rumorId) const {
  std::lock_guard<std::mutex> guard(m_mutex);  // critical section
  return m_rumors.count(rumorId) > 0;
}

std::ostream& RumorHolder::printStatistics(std::ostream& outStream) const {
  outStream << m_id << ": {"
            << "\n";
  for (const auto& stat : m_statistics) {
    outStream << "  " << s_enumKeyToString.at(stat.first) << ": " << stat.second
              << "\n";
  }
  outStream << "}";
  return outStream;
}

// OPERATORS
bool RumorHolder::operator==(const RumorHolder& other) const {
  return m_id == other.m_id;
}

}  // namespace RRS
