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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNCACHE_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNCACHE_H_

#include <deque>
#include <shared_mutex>
#include <unordered_set>

#include "Common.h"

namespace evmproj {
namespace filters {

class PendingTxnCache {
 public:
  void Append(const TxnHash &hash, EpochNumber epoch);

  void CleanupBefore(EpochNumber epoch);

  EpochNumber GetPendingTxnsFilterChanges(EpochNumber after_counter,
                                          PollResult &result);

 private:
  struct Item {
    EpochNumber counter;
    EpochNumber epoch;
    TxnHash hash;
  };

  EpochNumber m_counter = 0;

  std::deque<Item> m_items;

  std::unordered_set<TxnHash> m_index;

  std::shared_timed_mutex m_mutex;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNCACHE_H_
