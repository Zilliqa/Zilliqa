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

#include "Blacklist.h"
#include "libUtils/Logger.h"

using namespace std;

Blacklist::Blacklist() {}

Blacklist::~Blacklist() {}

Blacklist& Blacklist::GetInstance() {
  static Blacklist blacklist;
  return blacklist;
}

/// P2PComm may use this function
bool Blacklist::Exist(const boost::multiprecision::uint128_t& ip) {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  return (m_blacklistIP.end() != m_blacklistIP.find(ip));
}

/// Reputation Manager may use this function
void Blacklist::Add(const boost::multiprecision::uint128_t& ip) {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_blacklistIP.emplace(ip, true);
}

/// Reputation Manager may use this function
void Blacklist::Remove(const boost::multiprecision::uint128_t& ip) {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_blacklistIP.erase(ip);
}

/// Reputation Manager may use this function
void Blacklist::Clear() {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_blacklistIP.clear();
  LOG_GENERAL(INFO, "[blacklist] Blacklist cleared.");
}
