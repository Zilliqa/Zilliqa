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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_FILTERSIMPL_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_FILTERSIMPL_H_

#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "Common.h"

namespace evmproj {
namespace filters {
struct InstalledFilterBase {
  /// prevent parallel polling of the same filter
  std::mutex inProcessMutex;

  /// The epoch (or, for pending TXNs, internal counter) last seen by this
  /// filter's owner
  EpochNumber lastSeen = SEEN_NOTHING;
};

struct EventFilter : InstalledFilterBase {
  EventFilterParams params;
};

struct PendingTxnFilter : InstalledFilterBase {};

struct BlockFilter : InstalledFilterBase {};

class FilterAPIBackendImpl : public FilterAPIBackend {
 public:
  void SetEpochRange(uint64_t earliest, uint64_t latest) override;

  InstallResult InstallNewEventFilter(const Json::Value &params) override;

  InstallResult InstallNewBlockFilter() override;

  InstallResult InstallNewPendingTxnFilter() override;

  bool UninstallFilter(const std::string &filter_id) override;

  PollResult GetFilterChanges(const std::string &filter_id) override;

  explicit FilterAPIBackendImpl(TxCache &cache) : m_cache(cache) {}

 private:
  void GetEventFilterChanges(const std::string &filter_id, PollResult &result);
  void GetBlockFilterChanges(const std::string &filter_id, PollResult &result);
  void GetPendingTxnFilterChanges(const std::string &filter_id,
                                  PollResult &result);

  /// Metadata cache
  TxCache &m_cache;

  /// Epoch range that can be polled at the moment
  EpochNumber m_earliestEpoch = SEEN_NOTHING;
  EpochNumber m_latestEpoch = SEEN_NOTHING;

  /// Incremental counter for filter IDs
  uint64_t m_filterCounter = 0;

  /// Installed event filters
  std::unordered_map<FilterId, std::unique_ptr<EventFilter>> m_eventFilters;

  /// Installed pending TXNs filters
  std::unordered_map<FilterId, std::unique_ptr<PendingTxnFilter>>
      m_pendingTxnFilters;

  /// Installed block filters
  std::unordered_map<FilterId, std::unique_ptr<BlockFilter>> m_blockFilters;

  /// Parallel polling of different filters is allowed
  std::shared_timed_mutex m_installMutex;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_FILTERSIMPL_H_
