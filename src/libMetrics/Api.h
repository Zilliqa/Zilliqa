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
#include "libMetrics/internal/mixins.h"
#include "libMetrics/internal/scope.h"

// typedef or using which one do you prefer ??

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

using METRIC_ATTRIBUTE =
    std::map<std::string, opentelemetry::common::AttributeValue>;

#define INCREMENT_CALLS_COUNTER(COUNTER, FILTER_CLASS, ATTRIBUTE, VALUE) \
  if (zil::metrics::Filter::GetInstance().Enabled(                       \
          zil::metrics::FilterClass::FILTER_CLASS)) {                    \
    COUNTER.IncrementWithAttributes(1L, {{ATTRIBUTE, VALUE}});           \
  }

#define INCREMENT_CALLS_COUNTER2(COUNTER, FILTER_CLASS, ATTRIBUTE, VALUE, \
                                 ATTRIBUTE2, VALUE2)                      \
  if (zil::metrics::Filter::GetInstance().Enabled(                        \
          zil::metrics::FilterClass::FILTER_CLASS)) {                     \
    COUNTER.IncrementWithAttributes(                                      \
        1, {{ATTRIBUTE, VALUE}, {ATTRIBUTE2, VALUE2}});                   \
  }

#define INCREMENT_METHOD_CALLS_COUNTER(COUNTER, FILTER_CLASS) \
  if (zil::metrics::Filter::GetInstance().Enabled(            \
          zil::metrics::FilterClass::FILTER_CLASS)) {         \
    COUNTER.IncrementAttr({{"Method", __FUNCTION__}});        \
  }

#define METRICS_ENABLED(FILTER_CLASS)          \
  zil::metrics::Filter::GetInstance().Enabled( \
      zil::metrics::FilterClass::FILTER_CLASS)

#define INCREMENT_METHOD_CALLS_COUNTER2(COUNTER, FILTER_CLASS, METHOD) \
  if (zil::metrics::Filter::GetInstance().Enabled(                     \
          zil::metrics::FilterClass::FILTER_CLASS)) {                  \
    COUNTER.IncrementWithAttributes(1L, {{"Method", METHOD}});         \
  }

#endif  // ZILLIQA_SRC_LIBMETRICS_API_H_