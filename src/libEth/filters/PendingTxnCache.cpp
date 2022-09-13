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

#include <algorithm>
#include <cassert>
#include <mutex>

#include "PendingTxnCache.h"

#include "libUtils/Logger.h"

namespace evmproj {
namespace filters {

using UniqueLock = std::unique_lock<std::shared_timed_mutex>;
using SharedLock = std::shared_lock<std::shared_timed_mutex>;

PendingTxnCache::PendingTxnCache(size_t depth)
    : m_depth(static_cast<EpochNumber>(depth)) {
  assert(m_depth > 0);
}

void PendingTxnCache::Append(const TxnHash &hash, EpochNumber epoch) {
  UniqueLock lock(m_mutex);

  auto p = m_index.insert(std::make_pair(hash, true));
  if (!p.second) {
    LOG_GENERAL(INFO, "Ignoring pending txn duplicate");
    return;
  }

  auto last_epoch = GetLastEpoch();
  if (epoch < last_epoch) {
    LOG_GENERAL(WARNING, "Pending TXN epoch corrected to " << last_epoch);
    epoch = last_epoch;
  } else if (epoch > last_epoch) {
    Cleanup();
  }

  m_items.emplace_back();
  auto &item = m_items.back();
  item.counter = ++m_counter;
  item.epoch = epoch;
  item.hash = hash;
}

void PendingTxnCache::TransactionCommitted(const TxnHash &hash) {
  UniqueLock lock(m_mutex);

  auto it = m_index.find(hash);
  if (it == m_index.end()) {
    // ignore unknown txn
    return;
  }
  it->second = false;
}

EpochNumber PendingTxnCache::GetPendingTxnsFilterChanges(
    EpochNumber after_counter, PollResult &result) {
  result.result = Json::Value(Json::arrayValue);
  result.success = true;

  SharedLock lock(m_mutex);

  if (m_items.empty() || after_counter >= m_counter) {
    return after_counter;
  }

  Item item;
  item.counter = after_counter;

  auto it = std::upper_bound(
      m_items.begin(), m_items.end(), item,
      [](const Item &a, const Item &b) { return a.counter < b.counter; });

  for (; it != m_items.end(); ++it) {
    if (IsPending(it->hash)) {
      result.result.append(it->hash);
    }
  }

  return m_counter;
}

EpochNumber PendingTxnCache::GetLastEpoch() {
  if (m_items.empty()) {
    return SEEN_NOTHING;
  }
  return m_items.back().epoch;
}

bool PendingTxnCache::IsPending(const TxnHash &hash) {
  auto it = m_index.find(hash);
  if (it == m_index.end()) {
    LOG_GENERAL(WARNING, "Inconsisency in PendingTxnCache");
    return false;
  }
  return it->second;
}

void PendingTxnCache::Cleanup() {
  auto last = GetLastEpoch();
  if (last < 0) {
    return;
  }
  auto earliest = last - m_depth;
  if (earliest < 0) {
    return;
  }
  auto it = m_items.begin();
  for (; it != m_items.end(); ++it) {
    if (it->epoch >= earliest) {
      break;
    }
    m_index.erase(it->hash);
  }
  m_items.erase(m_items.begin(), it);
}

}  // namespace filters
}  // namespace evmproj
