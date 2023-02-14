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
#ifndef ZILLIQA_SRC_LIBMETRICS_INTERNAL_MIXINS_H_
#define ZILLIQA_SRC_LIBMETRICS_INTERNAL_MIXINS_H_

#include <map>
#include <string>

#include "libMetrics/Metrics.h"

namespace zil {
namespace metrics {

using METRIC_ATTRIBUTE =
    std::map<std::string, opentelemetry::common::AttributeValue>;

// Wrap an integer Counter

class I64Counter {
 public:
  I64Counter(zil::metrics::FilterClass fc, const std::string &name,
             const std::string &description, const std::string &units) {
    //   Metrics::GetInstance().AddCounterSumView(GetFullName(METRIC_FAMILY,
    //   name),
    //                                            "View of the Metric");
    if (Filter::GetInstance().Enabled(fc)) {
      m_theCounter = Metrics::GetMeter()->CreateUInt64Counter(
          GetFullName(METRIC_FAMILY, name), description, units);
    } else {
      m_theCounter = static_cast<uint64Counter_t>(
          new opentelemetry::metrics::NoopCounter<uint64_t>("test", "none",
                                                            "unitless"));
    }
  }

  void Increment() { m_theCounter->Add(1); }

  void IncrementWithAttributes(long val, const METRIC_ATTRIBUTE &attr) {
    m_theCounter->Add(val, attr);
  }

  virtual ~I64Counter() {}

  friend std::ostream &operator<<(std::ostream &os, const I64Counter &counter);

  uint64Counter_t &get() { return m_theCounter; }

 private:
  uint64Counter_t m_theCounter;
};

// wrap a double counter

class DoubleCounter {
 public:
  DoubleCounter(zil::metrics::FilterClass fc, const std::string &name,
                const std::string &description, const std::string &units) {
    if (Filter::GetInstance().Enabled(fc)) {
      m_theCounter = Metrics::GetMeter()->CreateDoubleCounter(
          GetFullName(METRIC_FAMILY, name), description, units);
    } else {
      m_theCounter = static_cast<doubleCounter_t>(
          new opentelemetry::metrics::NoopCounter<double>(
              GetFullName(METRIC_FAMILY, name), "none", "unitless"));
    }
  }

  void Increment() { m_theCounter->Add(1); }

  void IncrementWithAttributes(double val, const METRIC_ATTRIBUTE &attr) {
    m_theCounter->Add(val, attr);
  }

 private:
  doubleCounter_t m_theCounter;
};

// wrap a histogram

class DoubleHistogram {
 public:
  DoubleHistogram(zil::metrics::FilterClass fc, const std::string &name,
                  const std::list<double> &boundaries,
                  const std::string &description, const std::string &units)
      : m_boundaries(boundaries) {
    if (Filter::GetInstance().Enabled(fc)) {
      Metrics::GetInstance().AddCounterHistogramView(
          GetFullName(METRIC_FAMILY, name), boundaries, description);
      m_theCounter = Metrics::GetMeter()->CreateDoubleHistogram(
          GetFullName(METRIC_FAMILY, name), description, units);
    } else {
      m_theCounter = static_cast<doubleHistogram_t>(
          new opentelemetry::metrics::NoopHistogram<double>(
              GetFullName(METRIC_FAMILY, name), "none", "unitless"));
    }
  }

  void Record(double val) {
    auto context = opentelemetry::context::Context{};
    m_theCounter->Record(val, context);
  }

  void Record(double val, const METRIC_ATTRIBUTE &attr) {
    auto context = opentelemetry::context::Context{};
    m_theCounter->Record(val, attr, context);
  }

 private:
  std::list<double> m_boundaries;
  doubleHistogram_t m_theCounter;
};

class DoubleGauge {
 public:
  DoubleGauge(zil::metrics::FilterClass fc, const std::string &name,
              const std::string &description, const std::string &units, bool)
      : m_theGauge(Metrics::GetInstance().CreateDoubleGauge(
            zil::metrics::GetFullName(METRIC_FAMILY, name), description,
            units)),
        m_fc(fc) {}

  using Callback = std::function<void(Observable::Result &&result)>;

  void SetCallback(const Callback &cb) {
    if (zil::metrics::Filter::GetInstance().Enabled(m_fc))
      m_theGauge.SetCallback(cb);
  }

 private:
  zil::metrics::Observable m_theGauge;
  zil::metrics::FilterClass m_fc;
};

class I64Gauge {
 public:
  I64Gauge(zil::metrics::FilterClass fc, const std::string &name,
           const std::string &description, const std::string &units, bool)
      : m_theGauge(Metrics::GetInstance().CreateInt64Gauge(
            GetFullName(METRIC_FAMILY, name), description, units)),
        m_fc(fc) {}

  using Callback = std::function<void(Observable::Result &&result)>;

  void SetCallback(const Callback &cb) {
    if (zil::metrics::Filter::GetInstance().Enabled(m_fc))
      m_theGauge.SetCallback(cb);
  }

 private:
  zil::metrics::Observable m_theGauge;
  zil::metrics::FilterClass m_fc;
};

class I64UpDown {
 public:
  I64UpDown(zil::metrics::FilterClass fc, const std::string &name,
            const std::string &description, const std::string &units, bool)
      : m_theGauge(Metrics::GetInstance().CreateInt64UpDownMetric(
            GetFullName(METRIC_FAMILY, name), description, units)),
        m_fc(fc) {}

  using Callback = std::function<void(Observable::Result &&result)>;

  void SetCallback(const Callback &cb) {
    if (zil::metrics::Filter::GetInstance().Enabled(m_fc))
      m_theGauge.SetCallback(cb);
  }

 private:
  zil::metrics::Observable m_theGauge;
  zil::metrics::FilterClass m_fc;
};

class DoubleUpDown {
 public:
  DoubleUpDown(zil::metrics::FilterClass fc, const std::string &name,
               const std::string &description, const std::string &units, bool)
      : m_theGauge(Metrics::GetInstance().CreateDoubleUpDownMetric(
            GetFullName(METRIC_FAMILY, name), description, units)),
        m_fc(fc) {}

  using Callback = std::function<void(Observable::Result &&result)>;

  void SetCallback(const Callback &cb) {
    if (zil::metrics::Filter::GetInstance().Enabled(m_fc))
      m_theGauge.SetCallback(cb);
  }

 private:
  zil::metrics::Observable m_theGauge;
  zil::metrics::FilterClass m_fc;
};

template <typename T>
struct InstrumentWrapper : T {
  InstrumentWrapper(zil::metrics::FilterClass fc, const std::string &name,
                    const std::string &description, const std::string &units)
      : T(fc, name, description, units) {
    m_fc = fc;
  }

  // Special for the histogram.

  InstrumentWrapper(zil::metrics::FilterClass fc, const std::string &name,
                    const std::list<double> &list,
                    const std::string &description, const std::string &units)
      : T(fc, name, list, description, units) {
    m_fc = fc;
  }

  InstrumentWrapper(zil::metrics::FilterClass fc, const std::string &name,
                    const std::string &description, const std::string &units,
                    bool obs)
      : T(fc, name, description, units, obs) {
    m_fc = fc;
  }

  InstrumentWrapper &operator++() {
    if (Filter::GetInstance().Enabled(m_fc)) {
      T::Increment();
    }
    return *this;
  }

  // Prefix increment operator.
  InstrumentWrapper &operator++(int) {
    if (Filter::GetInstance().Enabled(m_fc)) {
      T::Increment();
    }
    return *this;
  }

  // Declare prefix and postfix decrement operators.
  InstrumentWrapper &operator--() {
    if (Filter::GetInstance().Enabled(m_fc)) {
      T::Decrement();
    }
    return *this;
  }  // Prefix d

  // decrement operator.
  InstrumentWrapper operator--(int) {
    InstrumentWrapper temp = *this;
    if (Filter::GetInstance().Enabled(m_fc)) {
      T::Decrement();
    }
    --*this;
    return temp;
  }

  void IncrementAttr(const METRIC_ATTRIBUTE &attr) {
    if (Filter::GetInstance().Enabled(m_fc)) {
      T::IncrementWithAttributes(1L, attr);
    }
  }

  void Increment(size_t steps) {
    if (Filter::GetInstance().Enabled(m_fc)) {
      while (steps--) T::Increment();
    }
  }

  void Decrement(size_t steps) {
    if (Filter::GetInstance().Enabled(m_fc)) {
      while (steps--) T::Decrement();
    }
  }

  bool Enabled() { return zil::metrics::Filter::GetInstance().Enabled(m_fc); }

 private:
  zil::metrics::FilterClass m_fc;
};

};  // namespace metrics
};  // namespace zil

#endif  // ZILLIQA_SRC_LIBMETRICS_INTERNAL_MIXINS_H_
