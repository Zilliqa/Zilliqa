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

#ifndef ZILLIQA_SRC_LIBUTILS_METRICS_H_
#define ZILLIQA_SRC_LIBUTILS_METRICS_H_

#include <cassert>

#include "common/MetricFilters.h"
#include "common/Singleton.h"

#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/sync_instruments.h"

class Metrics;

namespace opentelemetry::metrics {
class MeterProvider;
}

namespace zil {
namespace metrics {

namespace common = opentelemetry::common;
namespace metrics_api = opentelemetry::metrics;

using uint64Counter_t = std::unique_ptr<metrics_api::Counter<uint64_t>>;
using doubleCounter_t = std::unique_ptr<metrics_api::Counter<double>>;
using uint64Historgram_t = std::unique_ptr<metrics_api::Histogram<uint64_t>>;
using doubleHistogram_t = std::unique_ptr<metrics_api::Histogram<double>>;

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
    void Set(T value, const common::KeyValueIterable& attributes) {
      static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

      if constexpr (std::is_integral_v<T>) {
        // This looks like a bug in openTelemetry, need to investigate, clash
        // between uint64_t and long int should be unsigned, losing precision.

        SetImpl(static_cast<int64_t>(value), attributes);
      } else {
        SetImpl(static_cast<double>(value), attributes);
      }
    }

    template <class T, class U,
              std::enable_if_t<
                  common::detail::is_key_value_iterable<U>::value>* = nullptr>
    void Set(T value, const U& attributes) noexcept {
      Set(value, common::KeyValueIterableView<U>{attributes});
    }

    template <class T>
    void Set(T value, std::initializer_list<
                          std::pair<std::string_view, common::AttributeValue>>
                          attributes) noexcept {
      Set(value, opentelemetry::nostd::span<
                     const std::pair<std::string_view, common::AttributeValue>>{
                     attributes.begin(), attributes.end()});
    }

   private:
    friend Observable;  // for ctor

    Result(opentelemetry::metrics::ObserverResult& r) : m_result(r) {}

    void SetImpl(int64_t value, const common::KeyValueIterable& attributes);

    void SetImpl(double value, const common::KeyValueIterable& attributes);

    opentelemetry::metrics::ObserverResult& m_result;
  };

  using Callback = std::function<void(Result&& result)>;

  void SetCallback(Callback cb);

  /// Dtor resets callback in compliance to opentelemetry API
  ~Observable();

  // No copy-move because stability of 'this' ptr is required
  Observable(const Observable&) = delete;
  Observable(Observable&&) = delete;
  Observable& operator=(const Observable&) = delete;
  Observable& operator=(Observable&&) = delete;

 private:
  using observable_t = std::shared_ptr<metrics_api::ObservableInstrument>;

  // for ctor.
  friend Metrics;

  Observable(zil::metrics::FilterClass filter, observable_t ob)
      : m_filter(filter), m_observable(std::move(ob)) {
    assert(m_observable);
  }

  static void RawCallback(
      opentelemetry::metrics::ObserverResult observer_result, void* state);

  zil::metrics::FilterClass m_filter;
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

  zil::metrics::uint64Counter_t CreateInt64Metric(const std::string& family,
                                                  const std::string& name,
                                                  const std::string& desc,
                                                  std::string_view unit = "");

  zil::metrics::doubleCounter_t CreateDoubleMetric(const std::string& family,
                                                   const std::string& name,
                                                   const std::string& desc,
                                                   std::string_view unit = "");

  zil::metrics::uint64Historgram_t CreateUInt64Histogram(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::doubleHistogram_t CreateDoubleHistogram(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::Observable CreateInt64UpDownMetric(
      zil::metrics::FilterClass filter, const std::string& family,
      const std::string& name, const std::string& desc,
      std::string_view unit = "");

  zil::metrics::Observable CreateInt64Gauge(zil::metrics::FilterClass filter,
                                            const std::string& family,
                                            const std::string& name,
                                            const std::string& desc,
                                            std::string_view unit = "");

  zil::metrics::Observable CreateDoubleUpDownMetric(
      zil::metrics::FilterClass filter, const std::string& family,
      const std::string& name, const std::string& desc,
      std::string_view unit = "");

  zil::metrics::Observable CreateDoubleGauge(zil::metrics::FilterClass filter,
                                             const std::string& family,
                                             const std::string& name,
                                             const std::string& desc,
                                             std::string_view unit = "");

  zil::metrics::Observable CreateInt64ObservableCounter(
      zil::metrics::FilterClass filter, const std::string& family,
      const std::string& name, const std::string& desc,
      std::string_view unit = "");

  zil::metrics::Observable CreateDoubleObservableCounter(
      zil::metrics::FilterClass filter, const std::string& family,
      const std::string& name, const std::string& desc,
      std::string_view unit = "");

  /// Called on main() exit explicitly
  void Shutdown();

 private:
  void Init();

  std::shared_ptr<opentelemetry::metrics::MeterProvider> m_provider;
};

#define INCREMENT_CALLS_COUNTER(COUNTER, FILTER_CLASS, ATTRIBUTE, VALUE) \
  if (zil::metrics::Filter::GetInstance().Enabled(                       \
          zil::metrics::FilterClass::FILTER_CLASS)) {                    \
    COUNTER->Add(1, {{ATTRIBUTE, VALUE}});                               \
  }

#define INCREMENT_METHOD_CALLS_COUNTER(COUNTER, FILTER_CLASS) \
  if (zil::metrics::Filter::GetInstance().Enabled(            \
          zil::metrics::FilterClass::FILTER_CLASS)) {         \
    COUNTER->Add(1, {{"Method", __FUNCTION__}});              \
  }

#define INCREMENT_METHOD_CALLS_COUNTER2(COUNTER, FILTER_CLASS, METHOD) \
  if (zil::metrics::Filter::GetInstance().Enabled(                     \
          zil::metrics::FilterClass::FILTER_CLASS)) {                  \
    COUNTER->Add(1, {{"Method", METHOD}});                             \
  }

#endif  // ZILLIQA_SRC_LIBUTILS_METRICS_H_
