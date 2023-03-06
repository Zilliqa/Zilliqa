/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBMETRICS_METRICS_H_
#define ZILLIQA_SRC_LIBMETRICS_METRICS_H_

#include <cassert>
#include <list>
#include <string>

#ifndef HAVE_CPP_STDLIB
#define HAVE_CPP_STDLIB
#endif

#include <opentelemetry/metrics/provider.h>

#include "MetricFilters.h"
#include "common/Singleton.h"

#include "Common.h"

class Metrics;

namespace zil {
namespace metrics {

namespace common = opentelemetry::common;
namespace metrics_api = opentelemetry::metrics;

using uint64Counter_t = std::unique_ptr<metrics_api::Counter<uint64_t>>;
using doubleCounter_t = std::unique_ptr<metrics_api::Counter<double>>;
using doubleHistogram_t = std::unique_ptr<metrics_api::Histogram<double>>;
using observable_t = std::shared_ptr<metrics_api::ObservableInstrument>;

inline auto GetMeter(
    std::shared_ptr<opentelemetry::metrics::MeterProvider> &provider,
    const std::string &family) {
  return provider->GetMeter(family, METRIC_SCHEMA_VERSION, METRIC_SCHEMA);
}

inline std::string GetFullName(std::string_view family, std::string_view name) {
  std::string full_name;
  full_name.reserve(family.size() + name.size() + 1);
  full_name += family;
  full_name += "_";
  full_name += name;
  return full_name;
}

class Filter : public Singleton<Filter> {
 public:
  void init();

  bool Enabled(FilterClass to_test) {
    return m_mask & (1 << static_cast<int>(to_test));
  }

 private:
  uint64_t m_mask{};
};

class Observable {
 public:
  class Result {
   public:
    template <class T>
    void Set(T value, const common::KeyValueIterable &attributes) {
#if TESTING  // TODO : warn to logger
      static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
#endif
      if constexpr (std::is_integral_v<T>) {
        // This looks like a bug in openTelemetry, need to investigate, clash
        // between uint64_t and long int should be unsigned, losing precision.

        SetImpl(static_cast<int64_t>(value), attributes);
      } else {
        SetImpl(static_cast<double>(value), attributes);
      }
    }

    template <class T, class U,
              std::enable_if_t<common::detail::is_key_value_iterable<U>::value>
                  * = nullptr>
    void Set(T value, const U &attributes) noexcept {
      Set(value, common::KeyValueIterableView<U>{attributes});
    }

    template <class T>
    void Set(
        T value,
        std::initializer_list<std::pair<std::string, common::AttributeValue>>
            attributes) noexcept {
      Set(value, opentelemetry::nostd::span<
                     const std::pair<std::string, common::AttributeValue>>{
                     attributes.begin(), attributes.end()});
    }

   private:
    friend Observable;  // for ctor

    Result(opentelemetry::metrics::ObserverResult &r) : m_result(r) {}

    void SetImpl(int64_t value, const common::KeyValueIterable &attributes);

    void SetImpl(double value, const common::KeyValueIterable &attributes);

    opentelemetry::metrics::ObserverResult &m_result;
  };

  using Callback = std::function<void(Result &&result)>;

  void SetCallback(Callback cb);

  /// Dtor resets callback in compliance to opentelemetry API
  ~Observable();

  // No copy-move because stability of 'this' ptr is required
  Observable(const Observable &) = delete;

  Observable(Observable &&) = delete;

  Observable &operator=(const Observable &) = delete;

  Observable &operator=(Observable &&) = delete;

  Observable(observable_t ob) : m_observable(std::move(ob)) {
    assert(m_observable);
  }

 private:
  static void RawCallback(
      opentelemetry::metrics::ObserverResult observer_result, void *state);

  observable_t m_observable;
  Callback m_callback;
};

}  // namespace metrics
}  // namespace zil

// Class metrics updated to OpenTelemetry
//
// Uses a singleton to lazy load and initialise metrics if at least one metric
// is called.

class Metrics : public Singleton<Metrics> {
 public:
  Metrics();

  std::string Version() { return "Initial"; }

  zil::metrics::uint64Counter_t CreateInt64Metric(std::string_view name,
                                                  std::string_view desc,
                                                  std::string_view unit = "");

  zil::metrics::doubleCounter_t CreateDoubleMetric(std::string_view name,
                                                   std::string_view desc,
                                                   std::string_view unit = "");

  zil::metrics::doubleHistogram_t CreateDoubleHistogram(
      std::string_view name, std::string_view desc, std::string_view unit = "");

  zil::metrics::observable_t CreateInt64Gauge(std::string_view name,
                                              std::string_view desc,
                                              std::string_view unit = "");

  zil::metrics::observable_t CreateDoubleGauge(std::string_view name,
                                               std::string_view desc,
                                               std::string_view unit = "");

  zil::metrics::observable_t CreateInt64UpDownMetric(
      std::string_view name, std::string_view desc, std::string_view unit = "");

  zil::metrics::observable_t CreateDoubleUpDownMetric(
      std::string_view name, std::string_view desc, std::string_view unit = "");

  zil::metrics::observable_t CreateInt64ObservableCounter(
      std::string_view name, std::string_view desc, std::string_view unit = "");

  zil::metrics::observable_t CreateDoubleObservableCounter(
      std::string_view name, std::string_view desc, std::string_view unit = "");

  /// Called on main() exit explicitly
  void Init();
  void Shutdown();

  void AddCounterSumView(std::string_view name, std::string_view description);

  void AddCounterHistogramView(std::string_view name, std::vector<double> list,
                               std::string_view description);

  static std::shared_ptr<opentelemetry::metrics::Meter> GetMeter();

 private:
  friend class api_test;

  void InitPrometheus(const std::string &addr);

  void InitOTHTTP();

  void InitOtlpGrpc();

  void InitStdOut();

  void InitNoop();
};

#endif  // ZILLIQA_SRC_LIBMETRICS_METRICS_H_
