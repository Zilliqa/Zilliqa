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
#include <mutex>

#include "PendingTxnCache.h"

namespace evmproj {
namespace filters {

using UniqueLock = std::unique_lock<std::shared_timed_mutex>;
using SharedLock = std::shared_lock<std::shared_timed_mutex>;

void PendingTxnCache::Append(const TxnHash &hash, EpochNumber epoch) {
  UniqueLock lock(m_mutex);

  auto p = m_index.insert(hash);
  if (!p.second) {
    // TODO LOG warning duplicate
    return;
  }

  if (!m_items.empty() && epoch < m_items.back().epoch) {
    // TODO LOG warning
    epoch = m_items.back().epoch;
  }

  m_items.emplace_back();
  auto &item = m_items.back();
  item.counter = ++m_counter;
  item.epoch = epoch;
  item.hash = hash;
}

void PendingTxnCache::CleanupBefore(EpochNumber epoch) {
  UniqueLock lock(m_mutex);

  auto it = m_items.begin();
  auto end = m_items.end();
  for (; it != end; ++it) {
    if (it->epoch > epoch) {
      break;
    }
    m_index.erase(it->hash);
  }

  if (it != m_items.begin()) {
    m_items.erase(m_items.begin(), it);
  }
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
    result.result.append(it->hash);
  }

  return m_counter;
}

}  // namespace filters
}  // namespace evmproj
