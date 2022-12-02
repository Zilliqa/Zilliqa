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
#include "common/Singleton.h"
#include "common/MetricNames.h"
#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace common = opentelemetry::common;
namespace metrics_exporter = opentelemetry::exporter::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace metrics_api = opentelemetry::metrics;

namespace metrics {

using int64_t = std::unique_ptr<metrics_api::Counter<uint64_t>>;
using double_t = std::unique_ptr<metrics_api::Counter<double_t>>;
using int64Observable_t = std::shared_ptr<metrics_api::ObservableInstrument>;
using doubleObservable_t = std::shared_ptr<metrics_api::ObservableInstrument>;
using int64Historgram_t = std::unique_ptr<metrics_api::Histogram<uint64_t>>;
using doubleHistogram_t = std::unique_ptr<metrics_api::Histogram<double>>;

inline bool  IsEnabled(const int& to_test) {
  return METRIC_ZILLIQA_MASK & to_test;
}

}  // namespace metrics

// Class metrics updated to OpenTelemetry

class Metrics : public Singleton<Metrics> {
 public:
  Metrics();
  virtual ~Metrics(){};

  metrics::int64_t CreateInt64Metric(const std::string& family,
                                     const std::string& name,
                                     const std::string& desc,
                                     std::string_view unit = "");
  metrics::int64Observable_t CreateInt64UpDownMetric(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");
  metrics::int64Observable_t CreateInt64Gauge(const std::string& family,
                                              const std::string& name,
                                              const std::string& desc,
                                              std::string_view unit = "");
  metrics::doubleObservable_t CreateDoubleUpDownMetric(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");
  metrics::int64Observable_t CreateDoubleGauge(const std::string& family,
                                               const std::string& name,
                                               const std::string& desc,
                                               std::string_view unit = "");
  metrics::double_t CreateDoubleMetric(const std::string& family,
                                       const std::string& name,
                                       std::string_view unit = "");
  metrics::doubleHistogram_t CreateDoubleHistogram(const std::string& family,
                                                   const std::string& name,
                                                   const std::string& desc,
                                                   std::string_view unit = "");
  metrics::int64Historgram_t CreateUInt64Histogram(const std::string& family,
                                                   const std::string& name,
                                                   const std::string& desc,
                                                   std::string_view unit = "");
  metrics::int64Observable_t CreateInt64ObservableCounter(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");
  metrics::doubleObservable_t CreateDoubleObservableCounter(
      const std::string& family, const std::string& name,
      const std::string& desc, std::string_view unit = "");

 private:
  void Init();
  std::shared_ptr<metrics_api::MeterProvider> m_provider;
  bool m_status{false};
};

// A short hand test to save some repetitive coding.
// This macro works in conjunction with the constants value METRIC_ZILLIQA_MASK
// the mask is a 64 bit value each bit is reserved for a definition of value in
// common/MetricNames.h
// For the bit map to work a definiton should be a power of 2 in hex.
// 0 means nothing is set
// FFFF FFFF FFFF FFFF meaans all are set. by having each value as a
// power of 2 one can have 64x64 permuttions od the flags

#define     IF_MET_ENABLED(a) \
  if (METRIC_ZILLIQA_MASK & a)


#endif  // ZILLIQA_SRC_LIBUTILS_METRICS_H_
