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

bool ReputationManager::IsNodeBanned(
    const boost::multiprecision::uint128_t& IPAddress) {
  return (GetReputation(IPAddress) <= REPTHRESHOLD);
}

void ReputationManager::PunishNode(
    const boost::multiprecision::uint128_t& IPAddress, int32_t Penalty) {
  UpdateReputation(IPAddress, Penalty);
  if (!Blacklist::GetInstance().Exist(IPAddress) and IsNodeBanned(IPAddress)) {
    LOG_GENERAL(INFO, "Node " << IPConverter::ToStrFromNumericalIP(IPAddress)
                              << " banned.");
    Blacklist::GetInstance().Add(IPAddress);
  }
}

void ReputationManager::AwardAllNodes() {
  std::vector<boost::multiprecision::uint128_t> AllKnownIPs = GetAllKnownIP();
  for (const auto& ip : AllKnownIPs) {
    AwardNode(ip);
  }
}

void ReputationManager::AddNodeIfNotKnown(
    const boost::multiprecision::uint128_t& IPAddress) {
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  AddNodeIfNotKnownInternal(IPAddress);
}

void ReputationManager::AddNodeIfNotKnownInternal(
    const boost::multiprecision::uint128_t& IPAddress) {
  if (m_Reputations.find(IPAddress) == m_Reputations.end()) {
    m_Reputations.emplace(IPAddress, ScoreType::GOOD);
  }
}

int32_t ReputationManager::GetReputation(
    const boost::multiprecision::uint128_t& IPAddress) {
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  AddNodeIfNotKnownInternal(IPAddress);
  return m_Reputations[IPAddress];
}

void ReputationManager::Clear() {
  LOG_MARKER();
  std::lock_guard<std::mutex> lock(m_mutexReputations);
  m_Reputations.clear();
}

void ReputationManager::SetReputation(
    const boost::multiprecision::uint128_t& IPAddress,
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

void ReputationManager::UpdateReputation(
    const boost::multiprecision::uint128_t& IPAddress,
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

std::vector<boost::multiprecision::uint128_t>
ReputationManager::GetAllKnownIP() {
  std::lock_guard<std::mutex> lock(m_mutexReputations);

  std::vector<boost::multiprecision::uint128_t> AllKnownIPs;
  for (const auto& node : m_Reputations) {
    AllKnownIPs.emplace_back(node.first);
  }
  return AllKnownIPs;
}

void ReputationManager::AwardNode(
    const boost::multiprecision::uint128_t& IPAddress) {
  UpdateReputation(IPAddress, ScoreType::AWARD_FOR_GOOD_NODES);

  if (Blacklist::GetInstance().Exist(IPAddress) && !IsNodeBanned(IPAddress)) {
    LOG_GENERAL(INFO, "Node " << IPConverter::ToStrFromNumericalIP(IPAddress)
                              << " unbanned.");
    Blacklist::GetInstance().Remove(IPAddress);
  }
}