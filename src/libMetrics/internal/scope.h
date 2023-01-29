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
#ifndef ZILLIQA_SRC_LIBMETRICS_INTERNAL_SCOPE_H_
#define ZILLIQA_SRC_LIBMETRICS_INTERNAL_SCOPE_H_

#include "mixins.h"

namespace zil {
    namespace metrics {

        std::chrono::system_clock::time_point r_timer_start();

        double r_timer_end(std::chrono::system_clock::time_point start_time);

        class DoubleCounter;

        class DoubleHistogram;

        struct LatencyScopeMarker final {
            LatencyScopeMarker(
                    zil::metrics::InstrumentWrapper<zil::metrics::I64Counter>& metric,
                    zil::metrics::InstrumentWrapper<zil::metrics::DoubleHistogram>& latency,
                    zil::metrics::FilterClass fc, const char *file, const char *func);

            ~LatencyScopeMarker();

        private:
            std::string m_file;
            std::string m_func;
            uint64Counter_t& m_metric;
            InstrumentWrapper<DoubleHistogram>& m_latency;
            zil::metrics::FilterClass m_filterClass;
            std::chrono::system_clock::time_point m_startTime;

            LatencyScopeMarker(const LatencyScopeMarker &) = delete;

            LatencyScopeMarker &operator=(const LatencyScopeMarker &) = delete;
        };
    } // namespace metrics
}  // namespace zil

#endif  // ZILLIQA_SRC_LIBMETRICS_INTERNAL_SCOPE_H_
