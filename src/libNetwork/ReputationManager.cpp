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

#include "ReputationManager.h"

#include "Blacklist.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

ReputationManager::ReputationManager() {}

ReputationManager::~ReputationManager() {}

ReputationManager& ReputationManager::GetInstance() {
  static ReputationManager RM;
  return RM;
}

bool ReputationManager::IsNodeBanned(const uint128_t& IPAddress) {
  return (GetReputation(IPAddress) <= REPTHRESHOLD);
}

void ReputationManager::PunishNode(const uint128_t& IPAddress,
                                   int32_t Penalty) {
  UpdateReputation(IPAddress, Penalty);
  if (!Blacklist::GetInstance().Exist(IPAddress) and IsNodeBanned(IPAddress)) {
    LOG_GENERAL(INFO, "Node " << IPConverter::ToStrFromNumericalIP(IPAddress)
                              << " banned.");
    Blacklist::GetInstance().Add(IPAddress);
  }
}

void ReputationManager::AwardAllNodes() {
  std::vector<uint128_t> AllKnownIPs = GetAllKnownIP();
  for (const auto& ip : AllKnownIPs) {
    AwardNode(ip);
  }
}

void ReputationManager::AddNodeIfNotKnown(const uint128_t& IPAddress) {
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  AddNodeIfNotKnownInternal(IPAddress);
}

void ReputationManager::AddNodeIfNotKnownInternal(const uint128_t& IPAddress) {
  if (m_Reputations.find(IPAddress) == m_Reputations.end()) {
    m_Reputations.emplace(IPAddress, ScoreType::GOOD);
  }
}

int32_t ReputationManager::GetReputation(const uint128_t& IPAddress) {
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  AddNodeIfNotKnownInternal(IPAddress);
  return m_Reputations[IPAddress];
}

void ReputationManager::Clear() {
  LOG_MARKER();
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  m_Reputations.clear();
}

void ReputationManager::SetReputation(const uint128_t& IPAddress,
                                      const int32_t ReputationScore) {
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  AddNodeIfNotKnownInternal(IPAddress);

  if (ReputationScore > ScoreType::UPPERREPTHRESHOLD) {
    LOG_GENERAL(
        WARNING,
        "Reputation score too high. Exceed upper bound. ReputationScore: "
            << ReputationScore << ". Setting reputation to "
            << ScoreType::UPPERREPTHRESHOLD);
    m_Reputations[IPAddress] = ScoreType::UPPERREPTHRESHOLD;
    return;
  }

  m_Reputations[IPAddress] = ReputationScore;
}

void ReputationManager::UpdateReputation(const uint128_t& IPAddress,
                                         const int32_t ReputationScoreDelta) {
  int32_t NewRep = GetReputation(IPAddress);

  // Update result with score delta
  if (!(SafeMath<int32_t>::add(NewRep, ReputationScoreDelta, NewRep))) {
    LOG_GENERAL(WARNING, "Underflow/overflow detected.");
  }

  // Further deduct score if node is going to be ban
  if (NewRep <= REPTHRESHOLD && !IsNodeBanned(IPAddress)) {
    if (!(SafeMath<int32_t>::sub(
            NewRep, ScoreType::BAN_MULTIPLIER * ScoreType::AWARD_FOR_GOOD_NODES,
            NewRep))) {
      LOG_GENERAL(WARNING, "Underflow detected.");
    }
  }
  SetReputation(IPAddress, NewRep);
}

std::vector<uint128_t> ReputationManager::GetAllKnownIP() {
  std::lock_guard<std::mutex> lock(m_mutexReputations);

  std::vector<uint128_t> AllKnownIPs;
  for (const auto& node : m_Reputations) {
    AllKnownIPs.emplace_back(node.first);
  }
  return AllKnownIPs;
}

void ReputationManager::AwardNode(const uint128_t& IPAddress) {
  UpdateReputation(IPAddress, ScoreType::AWARD_FOR_GOOD_NODES);

  if (Blacklist::GetInstance().Exist(IPAddress) && !IsNodeBanned(IPAddress)) {
    LOG_GENERAL(INFO, "Node " << IPConverter::ToStrFromNumericalIP(IPAddress)
                              << " unbanned.");
    Blacklist::GetInstance().Remove(IPAddress);
  }
}
