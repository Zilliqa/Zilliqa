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

#include "scope.h"
#include <iostream>

namespace zil {
namespace metrics {

std::chrono::system_clock::time_point r_timer_start() {
  return std::chrono::system_clock::now();
}

double r_timer_end(std::chrono::system_clock::time_point start_time) {
  std::chrono::duration<double, std::micro> difference =
      std::chrono::system_clock::now() - start_time;
  return difference.count();
}

LatencyScopeMarker::LatencyScopeMarker(
    std::unique_ptr<metrics_api::Counter<uint64_t>> metric,
    InstrumentWrapper<DoubleHistogram>& latency, FilterClass fc,
    const char *file, const char *func)
    : m_file{file},
      m_func{func},
      m_metric(std::move(metric)),
      m_latency(latency),
      m_filterClass(fc),
      m_startTime(zil::metrics::r_timer_start()) {
      }

LatencyScopeMarker::~LatencyScopeMarker() {
  if (zil::metrics::Filter::GetInstance().Enabled(m_filterClass)) {
    try {
      double taken = zil::metrics::r_timer_end(m_startTime);
      METRIC_ATTRIBUTE counter_attr = {{"method", m_func}};
      m_metric->Add(1L, counter_attr);
      m_metric = nullptr;
      m_latency.Record(taken, counter_attr);
    } catch (...) {
      // TODO - Write some very specific Exception Handling.
      std::cout << "Brute force catch" << std::endl;
    }
  }
}
}  // namespace metrics
}  // namespace zil
