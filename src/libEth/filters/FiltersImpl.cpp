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

#include <string_view>

#include "FiltersImpl.h"
#include "FiltersUtils.h"

namespace evmproj {
namespace filters {

using UniqueLock = std::unique_lock<std::shared_timed_mutex>;
using SharedLock = std::shared_lock<std::shared_timed_mutex>;
using Lock = std::lock_guard<std::mutex>;

namespace {
const char *API_NOT_READY = "Filter API not ready";
const char *INVALID_FILTER_ID = "Invalid filter id";
const char *FILTER_NOT_FOUND = "Filter not found";
}  // namespace

void FilterAPIBackendImpl::SetEpochRange(uint64_t earliest, uint64_t latest) {
  assert(earliest <= latest);

  UniqueLock lock(m_installMutex);

  if (m_earliestEpoch >= static_cast<EpochNumber>(earliest) ||
      m_latestEpoch >= static_cast<EpochNumber>(latest)) {
    // XXX LOG warning
  }

  m_earliestEpoch = static_cast<EpochNumber>(earliest);
  m_latestEpoch = static_cast<EpochNumber>(latest);
}

FilterAPIBackend::InstallResult FilterAPIBackendImpl::InstallNewEventFilter(
    const Json::Value &params) {
  InstallResult ret;

  if (m_latestEpoch < 0) {
    ret.result = API_NOT_READY;
    return ret;
  }

  auto filter = std::make_unique<EventFilter>();

  if (!InitializeEventFilter(params, filter->params, ret.result)) {
    return ret;
  }

  UniqueLock lock(m_installMutex);

  ret.result = NewFilterId(++m_filterCounter, FilterType::EVENT_FILTER);

  m_eventFilters[ret.result] = std::move(filter);

  ret.success = true;
  return ret;
}

FilterAPIBackend::InstallResult FilterAPIBackendImpl::InstallNewBlockFilter() {
  InstallResult ret;

  if (m_latestEpoch < 0) {
    ret.result = API_NOT_READY;
  } else {
    UniqueLock lock(m_installMutex);

    ret.result = NewFilterId(++m_filterCounter, FilterType::BLK_FILTER);
    auto filter = std::make_unique<BlockFilter>();
    filter->lastSeen = m_latestEpoch - 1;
    m_blockFilters[ret.result] = std::move(filter);
    ret.success = true;
  }

  return ret;
}

FilterAPIBackend::InstallResult
FilterAPIBackendImpl::InstallNewPendingTxnFilter() {
  InstallResult ret;

  if (m_latestEpoch < 0) {
    ret.result = API_NOT_READY;
  } else {
    UniqueLock lock(m_installMutex);

    ret.result = NewFilterId(++m_filterCounter, FilterType::TXN_FILTER);
    auto filter = std::make_unique<PendingTxnFilter>();
    filter->lastSeen = SEEN_NOTHING;
    m_pendingTxnFilters[ret.result] = std::move(filter);
    ret.success = true;
  }

  return ret;
}

bool FilterAPIBackendImpl::UninstallFilter(const std::string &filter_id) {
  auto type = GuessFilterType(filter_id);

  if (type == FilterType::INVALID) {
    return false;
  }

  UniqueLock lock(m_installMutex);

  if (type == FilterType::EVENT_FILTER) {
    return (m_eventFilters.erase(filter_id) != 0);
  }

  if (type == FilterType::TXN_FILTER) {
    return (m_pendingTxnFilters.erase(filter_id) != 0);
  }

  return (m_blockFilters.erase(filter_id) != 0);
}

PollResult FilterAPIBackendImpl::GetFilterChanges(
    const std::string &filter_id) {
  PollResult ret;

  auto type = GuessFilterType(filter_id);

  switch (type) {
    case FilterType::EVENT_FILTER:
      GetEventFilterChanges(filter_id, ret);
      break;
    case FilterType::TXN_FILTER:
      GetPendingTxnFilterChanges(filter_id, ret);
      break;
    case FilterType::BLK_FILTER:
      GetBlockFilterChanges(filter_id, ret);
      break;
    default:
      ret.error = INVALID_FILTER_ID;
      break;
  }

  return ret;
}

void FilterAPIBackendImpl::GetEventFilterChanges(const std::string &filter_id,
                                                 PollResult &result) {
  SharedLock lock(m_installMutex);

  auto it = m_eventFilters.find(filter_id);
  if (it == m_eventFilters.end()) {
    result.error = FILTER_NOT_FOUND;
    return;
  }

  EventFilter &filter = *(it->second);
  result.result = Json::arrayValue;

  Lock personal_lock(filter.inProcessMutex);

  if (filter.lastSeen >= m_latestEpoch) {
    result.success = true;
    return;
  }

  filter.lastSeen =
      m_cache.GetEventFilterChanges(filter.lastSeen, filter.params, result);
}

void FilterAPIBackendImpl::GetPendingTxnFilterChanges(
    const std::string &filter_id, PollResult &result) {
  SharedLock lock(m_installMutex);

  auto it = m_pendingTxnFilters.find(filter_id);
  if (it == m_pendingTxnFilters.end()) {
    result.error = FILTER_NOT_FOUND;
    return;
  }

  PendingTxnFilter &filter = *(it->second);
  result.result = Json::arrayValue;

  Lock personal_lock(filter.inProcessMutex);

  if (filter.lastSeen >= m_latestEpoch) {
    result.success = true;
    return;
  }

  filter.lastSeen =
      m_cache.GetPendingTxnsFilterChanges(filter.lastSeen, result);
}

void FilterAPIBackendImpl::GetBlockFilterChanges(const std::string &filter_id,
                                                 PollResult &result) {
  SharedLock lock(m_installMutex);

  auto it = m_blockFilters.find(filter_id);
  if (it == m_blockFilters.end()) {
    result.error = FILTER_NOT_FOUND;
    return;
  }

  BlockFilter &filter = *(it->second);
  result.result = Json::arrayValue;

  Lock personal_lock(filter.inProcessMutex);

  if (filter.lastSeen >= m_latestEpoch) {
    result.success = true;
    return;
  }

  filter.lastSeen = m_cache.GetBlockFilterChanges(filter.lastSeen, result);
}

}  // namespace filters
}  // namespace evmproj
