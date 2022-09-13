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
#include <unordered_map>

#include "Common.h"

namespace evmproj {
namespace filters {

class PendingTxnCache {
 public:
  /// Ctor. Depth of cache in epochs
  explicit PendingTxnCache(size_t depth);

  /// Appends a new pending txn
  void Append(const TxnHash &hash, EpochNumber epoch);

  /// Set this txn as not pending, it will no longer be included into results
  /// \param hash Txn hash
  void TransactionCommitted(const TxnHash &hash);

  /// Returns filter changes since the last poll
  EpochNumber GetPendingTxnsFilterChanges(EpochNumber after_counter,
                                          PollResult &result);

 private:
  struct Item {
    /// Internal counter
    EpochNumber counter;

    /// Epoch of the TX
    EpochNumber epoch;

    /// Txn hash
    TxnHash hash;
  };

  EpochNumber GetLastEpoch();

  bool IsPending(const TxnHash &hash);

  void Cleanup();

  /// Cache depth in TX block epochs
  const EpochNumber m_depth;

  /// Internal counter which helps to avoid duplicates between the same filter
  /// polling calls
  EpochNumber m_counter = 0;

  /// Items ordered by counter
  std::deque<Item> m_items;

  /// TxnHash -> IsPending. Prevents from including committed txns into filter
  /// results
  std::unordered_map<TxnHash, bool> m_index;

  /// R-W lock. Allows for parallel polling
  std::shared_timed_mutex m_mutex;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNCACHE_H_
