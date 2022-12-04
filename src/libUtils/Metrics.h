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

#include "common/Constants.h"
#include "common/MetricNames.h"
#include "common/Singleton.h"
#include "magic_enum.hpp"
#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace common = opentelemetry::common;
namespace metrics_exporter = opentelemetry::exporter::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace metrics_api = opentelemetry::metrics;

namespace zil {
namespace metrics {

using int64_t = std::unique_ptr<metrics_api::Counter<uint64_t>>;
using double_t = std::unique_ptr<metrics_api::Counter<double_t>>;
using int64Observable_t = std::shared_ptr<metrics_api::ObservableInstrument>;
using doubleObservable_t = std::shared_ptr<metrics_api::ObservableInstrument>;
using int64Historgram_t = std::unique_ptr<metrics_api::Histogram<uint64_t>>;
using doubleHistogram_t = std::unique_ptr<metrics_api::Histogram<double>>;


class Filter {
 public:
  static void init() {
    // Pre cache powers of 2, this saves the developer having to work out what
    // is the value of each bit position, and only costs one indirection on test.
    int j = 0;
    for (auto& i : m_powers) {
      i = pow(2, j++);
      if (j==64){ // extend this if the mask increases in size
        break;
      }
    }
  }
  static bool Enabled(FilterClass to_test) {
    return METRIC_ZILLIQA_MASK & m_powers[to_test];
  }
 private:
  static std::array<int, magic_enum::enum_count<zil::metrics::FilterClass>()>
      m_powers;
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
  virtual ~Metrics(){};

  zil::metrics::int64_t CreateInt64Metric(const std::string& family,
                                          const std::string& name,
                                          const std::string& desc,
                                          std::string_view unit = "");

  zil::metrics::int64Observable_t CreateInt64UpDownMetric(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::int64Observable_t CreateInt64Gauge(const std::string& family,
                                                   const std::string& name,
                                                   const std::string& desc,
                                                   std::string_view unit = "");

  zil::metrics::doubleObservable_t CreateDoubleUpDownMetric(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::int64Observable_t CreateDoubleGauge(const std::string& family,
                                                    const std::string& name,
                                                    const std::string& desc,
                                                    std::string_view unit = "");

  zil::metrics::double_t CreateDoubleMetric(const std::string& family,
                                            const std::string& name,
                                            std::string_view unit = "");

  zil::metrics::doubleHistogram_t CreateDoubleHistogram(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::int64Historgram_t CreateUInt64Histogram(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::int64Observable_t CreateInt64ObservableCounter(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

  zil::metrics::doubleObservable_t CreateDoubleObservableCounter(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

 private:
  void Init();

  std::shared_ptr<metrics_api::MeterProvider> m_provider;
  bool m_status{false};
  zil::metrics::Filter m_tester;
};

#endif  // ZILLIQA_SRC_LIBUTILS_METRICS_H_
