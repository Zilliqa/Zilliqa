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

#include "Tracing.h"
#include "libMetrics/internal/mixins.h"
#include "libMetrics/internal/scope.h"

using Z_I64METRIC = zil::metrics::I64Counter;

// using Z_I64METRIC =
// zil::metrics::InstrumentWrapper<zil::metrics::I64Counter>; using Z_DBLMETRIC
// =
//     zil::metrics::InstrumentWrapper<zil::metrics::DoubleCounter>;
// using Z_DBLHIST =
//     zil::metrics::InstrumentWrapper<zil::metrics::DoubleHistogram>;
// using Z_DBLGAUGE =
// zil::metrics::InstrumentWrapper<zil::metrics::DoubleGauge>; using Z_I64GAUGE
// = zil::metrics::InstrumentWrapper<zil::metrics::I64Gauge>;

// Still virgins no use yet

// using Z_I64UPDOWN = zil::metrics::InstrumentWrapper<zil::metrics::I64UpDown>;
// using Z_DBLUPDOWN =
// zil::metrics::InstrumentWrapper<zil::metrics::DoubleUpDown>;

// Lazy

using Z_FL = zil::metrics::FilterClass;

#define DEFINE_I64_COUNTER(GETTER_NAME, FC, METRIC_NAME, DESCR, UNITS) \
  zil::metrics::I64Counter& GETTER_NAME() {                            \
    static auto c =                                                    \
        zil::metrics::CreateI64Counter(FC, METRIC_NAME, DESCR, UNITS); \
    return c;                                                          \
  }

#define DEFINE_DOUBLE_COUNTER(GETTER_NAME, FC, METRIC_NAME, DESCR, UNITS) \
  zil::metrics::DoubleCounter& GETTER_NAME() {                            \
    static auto c =                                                       \
        zil::metrics::CreateDoubleCounter(FC, METRIC_NAME, DESCR, UNITS); \
    return c;                                                             \
  }

#define INC_CALLS(COUNTER)                                     \
  if (COUNTER.Enabled()) {                                     \
    try {                                                      \
      COUNTER.IncrementAttr({{"calls", __FUNCTION__}});        \
    } catch (const std::exception& e) {                        \
      LOG_GENERAL(WARNING, "caught user error: " << e.what()); \
    } catch (...) {                                            \
      LOG_GENERAL(WARNING, "caught user error");               \
    }                                                          \
  }

#define INC_STATUS(COUNTER, KEY, VALUE)                                \
  if (COUNTER.Enabled()) {                                             \
    try {                                                              \
      COUNTER.IncrementAttr({{"Method", __FUNCTION__}, {KEY, VALUE}}); \
    } catch (const std::exception& e) {                                \
      LOG_GENERAL(WARNING, "caught user error: " << e.what());         \
    } catch (...) {                                                    \
      LOG_GENERAL(WARNING, "caught user error");                       \
    }                                                                  \
  }

#define DEFINE_I64_GAUGE(GETTER_NAME, FC, GAUGE_NAME, DESCR, UNITS, \
                         COUNTER_NAME_1)                            \
  int64_t& GETTER_NAME() {                                          \
    static auto g = zil::metrics::CreateGauge<int64_t>(             \
        FC, GAUGE_NAME, {#COUNTER_NAME_1}, DESCR, UNITS);           \
    return g.Get(0);                                                \
  }

#define DEFINE_GAUGE_GETTER(TYPE, INDEX, GETTER_NAME, COUNTER_NAME) \
  TYPE& GETTER_NAME##COUNTER_NAME() { return GETTER_NAME().Get(INDEX); }

#define DEFINE_I64_GAUGE_2(GETTER_NAME, FC, GAUGE_NAME, DESCR, UNITS,      \
                           COUNTER_NAME_1, COUNTER_NAME_2)                 \
  zil::metrics::GaugeT<int64_t>& GETTER_NAME() {                           \
    static auto g = zil::metrics::CreateGauge<int64_t>(                    \
        FC, GAUGE_NAME, {#COUNTER_NAME_1, #COUNTER_NAME_2}, DESCR, UNITS); \
    return g;                                                              \
  }                                                                        \
  DEFINE_GAUGE_GETTER(int64_t, 0, GETTER_NAME, COUNTER_NAME_1)             \
  DEFINE_GAUGE_GETTER(int64_t, 1, GETTER_NAME, COUNTER_NAME_2)

#define DEFINE_I64_GAUGE_3(GETTER_NAME, FC, GAUGE_NAME, DESCR, UNITS,        \
                           COUNTER_NAME_1, COUNTER_NAME_2, COUNTER_NAME_3)   \
  zil::metrics::GaugeT<int64_t>& GETTER_NAME() {                             \
    static auto g = zil::metrics::CreateGauge<int64_t>(                      \
        FC, GAUGE_NAME, {#COUNTER_NAME_1, #COUNTER_NAME_2, #COUNTER_NAME_3}, \
        DESCR, UNITS);                                                       \
    return g;                                                                \
  }                                                                          \
  DEFINE_GAUGE_GETTER(int64_t, 0, GETTER_NAME, COUNTER_NAME_1)               \
  DEFINE_GAUGE_GETTER(int64_t, 1, GETTER_NAME, COUNTER_NAME_2)               \
  DEFINE_GAUGE_GETTER(int64_t, 2, GETTER_NAME, COUNTER_NAME_3)

// DEFINE_I64_GAUGE(QQQ, Z_FL::ACCOUNTSTORE_EVM, "bbb", "desc", "", ololo);
//
// DEFINE_I64_GAUGE_2(Zzz, Z_FL::ACCOUNTSTORE_EVM, "aaa", "desc", "", Sos,
//                    Schnaps);
//
// DEFINE_I64_GAUGE_3(Zzz3, Z_FL::ACCOUNTSTORE_EVM, "qqq", "desc", "", Sos,
//                    Schnaps, Sis);

#define DEFINE_HISTOGRAM(GETTER_NAME, FC, NAME, DESCR, UNITS, ...) \
  zil::metrics::DoubleHistogram& GETTER_NAME() {                   \
    static auto h = zil::metrics::CreateDoubleHistogram(           \
        FC, NAME, {__VA_ARGS__}, DESCR, UNITS);                    \
    return h;                                                      \
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
