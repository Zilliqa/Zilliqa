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
bool Blacklist::Exist(const NodeKey& key, const bool strict) {
  if (!m_enabled) {
    return false;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  const auto& bl = m_BlackListNode.find(key);
  if (bl != m_BlackListNode.end()) {
    if (strict) {
      // always return exist when strict, must be checked while sending message
      return true;
    }

    return bl->second;
  }
  return false;
}


void Blacklist::Add(const NodeKey& key, const bool strict,
                    const bool ignoreWhitelist) {
  if (!m_enabled) {
    return;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  if (m_whiteListNode.end() == m_whiteListNode.find(key)) {
    const auto& res = m_BlackListNode.emplace(key, strict);
    if (!res.second) {
      res.first->second = strict;
    }
  } else {
    if (ignoreWhitelist != 0) {
      const auto& res = m_BlackListNode.emplace(key, strict);
      // already existed, then over-ride strictness
      if (!res.second) {
        res.first->second = strict;
      }
    } else {
      LOG_GENERAL(
          INFO, "Whitelisted IP: " << IPConverter::ToStrFromNumericalIP(key.ip)
                                   << " : " << key.port);
    }
  }
}


/// Reputation Manager may use this function
void Blacklist::Remove(const NodeKey& key) {
  if (!m_enabled) {
    return;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_BlackListNode.erase(key);
}

/// Reputation Manager may use this function
void Blacklist::Clear() {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  m_BlackListNode.clear();
  LOG_GENERAL(INFO, "Blacklist cleared");
}

void Blacklist::Pop(unsigned int num_to_pop) {
  if (!m_enabled) {
    return;
  }

  lock_guard<mutex> g(m_mutexBlacklistIP);
  LOG_GENERAL(INFO, "Num of nodes in blacklist: " << m_BlackListNode.size());

  unsigned int counter = 0;
  for (auto it = m_BlackListNode.begin(); it != m_BlackListNode.end();) {
    if (counter < num_to_pop) {
      it = m_BlackListNode.erase(it);
      counter++;
    } else {
      break;
    }
  }

  LOG_GENERAL(INFO, "Removed " << counter << " nodes from blacklist");
}

unsigned int Blacklist::SizeOfBlacklist() {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  return m_BlackListNode.size();
}

void Blacklist::Enable(const bool enable) {
  if (!enable) {
    Clear();
  }

  m_enabled = enable;
}

bool Blacklist::IsEnabled() { return m_enabled; }

bool Blacklist::Whitelist(const NodeKey& key) {
  if (!m_enabled) {
    return false;
  }
  lock_guard<mutex> g(m_mutexBlacklistIP);
  return m_whiteListNode.emplace(key).second;
}

bool Blacklist::RemoveFromWhitelist(const NodeKey& key) {
  if (!m_enabled) {
    return false;
  }
  lock_guard<mutex> g(m_mutexBlacklistIP);
  return (m_whiteListNode.erase(key) > 0);
}

bool Blacklist::IsWhitelistedIP(const NodeKey& key) {
  lock_guard<mutex> g(m_mutexBlacklistIP);
  return m_whiteListNode.end() != m_whiteListNode.find(key);
}

// TODO : SW
bool Blacklist::WhitelistSeed(const NodeKey& key) {
  if (!m_enabled) {
    return false;
  }

  {
    // Incase it was already blacklisted, remove it.
    lock_guard<mutex> g(m_mutexBlacklistIP);
    m_BlackListNode.erase(key);
  }

  lock_guard<mutex> g(m_mutexWhitelistedSeedsIP);
  return m_whitelistedSeedsNodes.emplace(key).second;
}

// TODO : SW
bool Blacklist::RemoveFromWhitelistedSeeds(const NodeKey& key) {
  if (!m_enabled) {
    return false;
  }
  lock_guard<mutex> g(m_mutexWhitelistedSeedsIP);
  return (m_whitelistedSeedsNodes.erase(key) > 0);
}

// TODO : SW
bool Blacklist::IsWhitelistedSeed(const NodeKey& key) {
  lock_guard<mutex> g(m_mutexWhitelistedSeedsIP);
  return m_whitelistedSeedsNodes.end() != m_whitelistedSeedsNodes.find(key);
}
