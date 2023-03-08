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
#ifndef ZILLIQA_SRC_LIBMETRICS_API_H_
#define ZILLIQA_SRC_LIBMETRICS_API_H_

#include "Metrics.h"
#include "Tracing.h"
#include "libMetrics/internal/mixins.h"
#include "libMetrics/internal/scope.h"

using Z_I64METRIC = zil::metrics::InstrumentWrapper<zil::metrics::I64Counter>;
using Z_DBLMETRIC =
    zil::metrics::InstrumentWrapper<zil::metrics::DoubleCounter>;
using Z_DBLHIST =
    zil::metrics::InstrumentWrapper<zil::metrics::DoubleHistogram>;
using Z_DBLGAUGE = zil::metrics::InstrumentWrapper<zil::metrics::DoubleGauge>;
using Z_I64GAUGE = zil::metrics::InstrumentWrapper<zil::metrics::I64Gauge>;

// Still virgins no use yet

using Z_I64UPDOWN = zil::metrics::InstrumentWrapper<zil::metrics::I64UpDown>;
using Z_DBLUPDOWN = zil::metrics::InstrumentWrapper<zil::metrics::DoubleUpDown>;

// Lazy

using Z_FL = zil::metrics::FilterClass;

// Yaron asked us to flesh these out with better catch.

#define INC_CALLS(COUNTER)                              \
  if (COUNTER.Enabled()) {                              \
    try {                                               \
      COUNTER.IncrementAttr({{"calls", __FUNCTION__}}); \
    } catch (...) {                                     \
      LOG_GENERAL(WARNING, "caught user error");        \
    }                                                   \
  }

#define INC_STATUS(COUNTER, KEY, VALUE)                                \
  if (COUNTER.Enabled()) {                                             \
    try {                                                              \
      COUNTER.IncrementAttr({{"Method", __FUNCTION__}, {KEY, VALUE}}); \
    } catch (...) {                                                    \
      LOG_GENERAL(WARNING, "caught user error");                       \
    }                                                                  \
  }

#define TRACE(FILTER_CLASS) \
  auto span = zil::trace::Tracing::CreateSpan(FILTER_CLASS, __FUNCTION__);

#define METRICS_ENABLED(FILTER_CLASS)          \
  zil::metrics::Filter::GetInstance().Enabled( \
      zil::metrics::FilterClass::FILTER_CLASS)

namespace zil {
namespace observability {
namespace api {
void EventMetricTrace(const std::string msg, std::string funcName, int line,
                      int errno);
void EventTrace(const std::string& eventname, const std::string& topic,
                const std::string& value);
}  // namespace api
}  // namespace observability
}  // namespace zil

#define TRACE_ERROR(MSG) \
  zil::observability::api::EventMetricTrace(MSG, __FUNCTION__, __LINE__, 0);

#define TRACE_EVENT(EVENT, TOPIC, VALUE) \
  zil::observability::api::EventTrace(EVENT, TOPIC, VALUE);

#endif  // ZILLIQA_SRC_LIBMETRICS_API_H_
