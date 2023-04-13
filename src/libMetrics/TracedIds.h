/*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBMETRICS_TRACEDIDS_H_
#define ZILLIQA_SRC_LIBMETRICS_TRACEDIDS_H_

#include "common/Singleton.h"

#include <mutex>
#include <string>

struct TracedIds : public Singleton<TracedIds> {
  std::string GetCurrentEpochSpanIds() const {
    std::unique_lock<decltype(m_currentEpochSpanIdsMutex)> guard(
        m_currentEpochSpanIdsMutex);

    return m_currentEpochSpanIds;
  }
  template <typename StringT>
  void SetCurrentEpochSpanIds(StringT&& spanIds) {
    std::unique_lock<decltype(m_currentEpochSpanIdsMutex)> guard(
        m_currentEpochSpanIdsMutex);

    m_currentEpochSpanIds = std::forward<StringT>(spanIds);
  }

  std::string GetConsensusSpanIds() const {
    std::unique_lock<decltype(m_consensusSpanIdsMutex)> guard(
        m_consensusSpanIdsMutex);

    return m_consensusSpanIds;
  }
  template <typename StringT>
  void SetConsensusSpanIds(StringT&& spanIds) {
    std::unique_lock<decltype(m_consensusSpanIdsMutex)> guard(
        m_consensusSpanIdsMutex);

    m_consensusSpanIds = std::forward<StringT>(spanIds);
  }

 private:
  mutable std::mutex m_currentEpochSpanIdsMutex;
  std::string m_currentEpochSpanIds;

  mutable std::mutex m_consensusSpanIdsMutex;
  std::string m_consensusSpanIds;
};

#endif  // ZILLIQA_SRC_LIBMETRICS_TRACEDIDS_H_
