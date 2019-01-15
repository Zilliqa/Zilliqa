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

#include "Blacklist.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

using namespace std;

Blacklist::Blacklist() : m_enabled(true) {}

Blacklist::~Blacklist() {}

Blacklist& Blacklist::GetInstance() {
  static Blacklist blacklist;
  return blacklist;
}

/// P2PComm may use this function
bool Blacklist::Exist(const boost::multiprecision::uint128_t& ip) {
  if (!m_enabled) {
    return false;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  return (m_blacklistIP.end() != m_blacklistIP.find(ip) &&
          (m_excludedIP.end() == m_excludedIP.find(ip)));
}

/// Reputation Manager may use this function
void Blacklist::Add(const boost::multiprecision::uint128_t& ip) {
  if (!m_enabled) {
    return;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  if (m_excludedIP.end() == m_excludedIP.find(ip)) {
    m_blacklistIP.emplace(ip, true);
  } else {
    LOG_GENERAL(INFO, "Excluding " << IPConverter::ToStrFromNumericalIP(ip)
                                   << " from Blacklist");
  }
}

/// Reputation Manager may use this function
void Blacklist::Remove(const boost::multiprecision::uint128_t& ip) {
  if (!m_enabled) {
    return;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_blacklistIP.erase(ip);
}

/// Reputation Manager may use this function
void Blacklist::Clear() {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_blacklistIP.clear();
  LOG_GENERAL(INFO, "[blacklist] Blacklist cleared.");
}

void Blacklist::Enable(const bool enable) {
  if (!enable) {
    Clear();
  }

  m_enabled = enable;
}

void Blacklist::Exclude(const boost::multiprecision::uint128_t& ip) {
  if (!m_enabled) {
    return;
  }
  m_excludedIP.emplace(ip);
}
