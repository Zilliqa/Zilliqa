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
#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace common = opentelemetry::common;
namespace metrics_exporter = opentelemetry::exporter::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace nostd = opentelemetry::nostd;
namespace metrics_api = opentelemetry::metrics;

namespace metrics {
using int64_t =
    nostd::unique_ptr<metrics_api::Counter<uint64_t>>;
using double_t =
    nostd::unique_ptr<metrics_api::Counter<double_t>>;
}

class Metrics : public Singleton<Metrics> {
 public:
  Metrics();

  metrics::int64_t CreateInt64Metric(const std::string& family,const std::string& name,const std::string& desc);
  metrics::double_t CreateDoubleMetric(const std::string& family,const std::string& name);

 private:
  void Init();
  std::shared_ptr<metrics_api::MeterProvider>   m_provider;
  bool m_status {false};
};



#endif  // ZILLIQA_SRC_LIBUTILS_METRICS_H_

