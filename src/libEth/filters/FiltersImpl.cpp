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

#include <boost/asio/steady_timer.hpp>

#include "FiltersImpl.h"
#include "FiltersUtils.h"
#include "libUtils/Logger.h"

namespace evmproj {
namespace filters {

using UniqueLock = std::unique_lock<std::shared_timed_mutex>;
using SharedLock = std::shared_lock<std::shared_timed_mutex>;
using Lock = std::lock_guard<std::mutex>;

namespace {

const char *API_NOT_READY = "Filter API not ready";
const char *INVALID_FILTER_ID = "Invalid filter id";
const char *FILTER_NOT_FOUND = "Filter not found";

std::chrono::seconds Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
      boost::asio::steady_timer::clock_type::now().time_since_epoch());
}

static const std::chrono::seconds FILTER_EXPIRE_TIME(86400);

}  // namespace

void FilterAPIBackendImpl::SetEpochRange(uint64_t earliest, uint64_t latest) {
  assert(earliest <= latest);

  UniqueLock lock(m_installMutex);

  if (m_earliestEpoch > static_cast<EpochNumber>(earliest) ||
      m_latestEpoch > static_cast<EpochNumber>(latest)) {
    LOG_GENERAL(WARNING, "Inconsistency in epochs");
  } else {
    m_earliestEpoch = static_cast<EpochNumber>(earliest);
    m_latestEpoch = static_cast<EpochNumber>(latest);
  }

  auto now = Now();
  auto it = m_expiration.begin();
  for (; it != m_expiration.end(); ++it) {
    if (it->first > now) {
      break;
    }
    UninstallFilter(it->second, GuessFilterType(it->second));
  }
  m_expiration.erase(m_expiration.begin(), it);
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

  filter->expireTime = Now() + FILTER_EXPIRE_TIME;
  m_expiration.emplace(std::make_pair(filter->expireTime, ret.result));

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

    filter->expireTime = Now() + FILTER_EXPIRE_TIME;
    m_expiration.emplace(std::make_pair(filter->expireTime, ret.result));

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

    filter->expireTime = Now() + FILTER_EXPIRE_TIME;
    m_expiration.emplace(std::make_pair(filter->expireTime, ret.result));

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

  return UninstallFilter(filter_id, type);
}

bool FilterAPIBackendImpl::UninstallFilter(const std::string &filter_id,
                                           FilterType type) {
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

  std::chrono::seconds expireTime{};

  switch (type) {
    case FilterType::EVENT_FILTER:
      GetEventFilterChanges(filter_id, ret, expireTime);
      break;
    case FilterType::TXN_FILTER:
      GetPendingTxnFilterChanges(filter_id, ret, expireTime);
      break;
    case FilterType::BLK_FILTER:
      GetBlockFilterChanges(filter_id, ret, expireTime);
      break;
    default:
      ret.error = INVALID_FILTER_ID;
      break;
  }

  // shift expiration time
  if (expireTime != std::chrono::seconds::zero()) {
    auto newExpireTime = Now() + FILTER_EXPIRE_TIME;

    if (newExpireTime != expireTime) {
      auto p = std::make_pair(expireTime, filter_id);
      m_expiration.erase(p);
      p.first = newExpireTime;
      m_expiration.emplace(std::move(p));
    }
  }

  return ret;
}

void FilterAPIBackendImpl::GetEventFilterChanges(
    const std::string &filter_id, PollResult &result,
    std::chrono::seconds &expireTime, bool ignore_last_seen_cursor) {
  SharedLock lock(m_installMutex);

  auto it = m_eventFilters.find(filter_id);
  if (it == m_eventFilters.end()) {
    result.error = FILTER_NOT_FOUND;
    return;
  }

  EventFilter &filter = *(it->second);
  result.result = Json::arrayValue;

  Lock personal_lock(filter.inProcessMutex);

  if (ignore_last_seen_cursor) {
    std::ignore =
        m_cache.GetEventFilterChanges(SEEN_NOTHING, filter.params, result);
    return;
  }

  expireTime = filter.expireTime;

  if (filter.lastSeen >= m_latestEpoch) {
    result.success = true;
    return;
  }

  filter.lastSeen =
      m_cache.GetEventFilterChanges(filter.lastSeen, filter.params, result);
}

void FilterAPIBackendImpl::GetPendingTxnFilterChanges(
    const std::string &filter_id, PollResult &result,
    std::chrono::seconds &expireTime) {
  SharedLock lock(m_installMutex);

  auto it = m_pendingTxnFilters.find(filter_id);
  if (it == m_pendingTxnFilters.end()) {
    result.error = FILTER_NOT_FOUND;
    return;
  }

  PendingTxnFilter &filter = *(it->second);
  result.result = Json::arrayValue;

  Lock personal_lock(filter.inProcessMutex);

  expireTime = filter.expireTime;

  if (filter.lastSeen >= m_latestEpoch) {
    result.success = true;
    return;
  }

  filter.lastSeen =
      m_cache.GetPendingTxnsFilterChanges(filter.lastSeen, result);
}

void FilterAPIBackendImpl::GetBlockFilterChanges(
    const std::string &filter_id, PollResult &result,
    std::chrono::seconds &expireTime) {
  SharedLock lock(m_installMutex);

  auto it = m_blockFilters.find(filter_id);
  if (it == m_blockFilters.end()) {
    result.error = FILTER_NOT_FOUND;
    return;
  }

  BlockFilter &filter = *(it->second);
  result.result = Json::arrayValue;

  Lock personal_lock(filter.inProcessMutex);

  expireTime = filter.expireTime;

  if (filter.lastSeen >= m_latestEpoch) {
    result.success = true;
    return;
  }

  filter.lastSeen = m_cache.GetBlockFilterChanges(filter.lastSeen, result);
}

PollResult FilterAPIBackendImpl::GetFilterLogs(const FilterId &filter_id) {
  PollResult ret;

  if (GuessFilterType(filter_id) != FilterType::EVENT_FILTER) {
    ret.error = INVALID_FILTER_ID;
  } else {
    std::chrono::seconds dummy;
    GetEventFilterChanges(filter_id, ret, dummy, true);
  }

  return ret;
}

PollResult FilterAPIBackendImpl::GetLogs(const Json::Value &params) {
  PollResult ret;

  if (m_latestEpoch < 0) {
    ret.error = API_NOT_READY;
    return ret;
  }

  EventFilterParams filter;

  if (!InitializeEventFilter(params, filter, ret.error)) {
    return ret;
  }

  std::ignore = m_cache.GetEventFilterChanges(SEEN_NOTHING, filter, ret);

  return ret;
}

}  // namespace filters
}  // namespace evmproj
