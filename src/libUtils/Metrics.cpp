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

#include "Metrics.h"

#include <vector>

#include <boost/algorithm/string.hpp>
#include <opentelemetry/sdk/resource/resource.h>
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"

#include "common/Constants.h"
#include "libUtils/Logger.h"

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace metrics_exporter = opentelemetry::exporter::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace otlp_exporter = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;

// The OpenTelemetry Metrics Interface.

Metrics::Metrics() { Init(); }

void Metrics::Init() {
  std::string cmp(METRIC_ZILLIQA_PROVIDER);

  if (cmp == "PROMETHEUS"){
    InitPrometheus();
  }
  else if (cmp == "OTLPHTTP") {
      InitOTHTTP();
  }
  else {
      InitStdOut(); // our favourite
  }
}

void Metrics::InitStdOut()
{
    std::string name = "zilliqa";

    std::unique_ptr<metrics_sdk::PushMetricExporter> exporter{
            new metrics_exporter::OStreamMetricExporter};

    std::string version{"1.2.0"};
    std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

    std::string library_name    = "metrics.cpp";
    std::string library_version = "0.0.1";
    std::string schema_url      = "https://zilliqa/schemas/1.2.0";
    auto instrumentation_scope =
            opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(library_name, library_version, schema_url);

    resource::ResourceAttributes attributes = {{"service.name", "zilliqa-cpp"},
                                               {"version", (uint32_t)1}};
    auto resource = resource::Resource::Create(attributes);

    // Initialize and set the global MeterProvider
    metrics_sdk::PeriodicExportingMetricReaderOptions options;
    options.export_interval_millis = std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
    options.export_timeout_millis = std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);
    std::unique_ptr<metrics_sdk::MetricReader> reader{
            new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter), options)};
    auto provider = std::shared_ptr<metrics_api::MeterProvider>(new metrics_sdk::MeterProvider());
    auto p        = std::static_pointer_cast<metrics_sdk::MeterProvider>(provider);
    p->AddMetricReader(std::move(reader));

    // counter view
    std::string counter_name = name + "_counter";
    std::unique_ptr<metrics_sdk::InstrumentSelector> instrument_selector{
            new metrics_sdk::InstrumentSelector(metrics_sdk::InstrumentType::kCounter, counter_name)};
    std::unique_ptr<metrics_sdk::MeterSelector> meter_selector{
            new metrics_sdk::MeterSelector(name, version, schema)};
    std::unique_ptr<metrics_sdk::View> sum_view{
            new metrics_sdk::View{name, "description", metrics_sdk::AggregationType::kSum}};
    p->AddView(std::move(instrument_selector), std::move(meter_selector), std::move(sum_view));

    // observable counter view
    std::string observable_counter_name = name + "_observable_counter";
    std::unique_ptr<metrics_sdk::InstrumentSelector> observable_instrument_selector{
            new metrics_sdk::InstrumentSelector(metrics_sdk::InstrumentType::kObservableCounter,
                                               observable_counter_name)};
    std::unique_ptr<metrics_sdk::MeterSelector> observable_meter_selector{
            new metrics_sdk::MeterSelector(name, version, schema)};
    std::unique_ptr<metrics_sdk::View> observable_sum_view{
            new metrics_sdk::View{name, "test_description", metrics_sdk::AggregationType::kSum}};
    p->AddView(std::move(observable_instrument_selector), std::move(observable_meter_selector),
               std::move(observable_sum_view));

    // histogram view
    std::string histogram_name = name + "_histogram";
    std::unique_ptr<metrics_sdk::InstrumentSelector> histogram_instrument_selector{
            new metrics_sdk::InstrumentSelector(metrics_sdk::InstrumentType::kHistogram, histogram_name)};
    std::unique_ptr<metrics_sdk::MeterSelector> histogram_meter_selector{
            new metrics_sdk::MeterSelector(name, version, schema)};
    std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> aggregation_config{
            new opentelemetry::sdk::metrics::HistogramAggregationConfig};
    static_cast<opentelemetry::sdk::metrics::HistogramAggregationConfig *>(aggregation_config.get())
            ->boundaries_ = std::list<double>{0.0,    50.0,   100.0,  250.0,   500.0,  750.0,
                                              1000.0, 2500.0, 5000.0, 10000.0, 20000.0};
    std::unique_ptr<metrics_sdk::View> histogram_view{new metrics_sdk::View{
            name, "description", metrics_sdk::AggregationType::kHistogram, aggregation_config}};
    p->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
               std::move(histogram_view));
    m_provider = p;
    opentelemetry::metrics::Provider::SetMeterProvider(m_provider);
    zil::metrics::Filter::GetInstance().init();
}


void Metrics::InitOTHTTP() {
  metrics_sdk::PeriodicExportingMetricReaderOptions opts;

  std::string addr{std::string(METRIC_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(METRIC_ZILLIQA_PORT)};

  otlp_exporter::OtlpHttpMetricExporterOptions options;

  if (!addr.empty()) {
    options.url = "http://" + addr + "/v1/metrics";
  }
  auto exporter = otlp_exporter::OtlpHttpMetricExporterFactory::Create(options);

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  // Initialize and set the global MeterProvider

  opts.export_interval_millis = std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
  opts.export_timeout_millis = std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);

    std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     opts)};
  m_provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metrics_sdk::MeterProvider());
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(m_provider);
  p->AddMetricReader(std::move(reader));

  metrics_api::Provider::SetMeterProvider(m_provider);
}

void Metrics::InitPrometheus() {
  std::string addr{std::string(METRIC_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(METRIC_ZILLIQA_PORT)};

  metrics_exporter::PrometheusExporterOptions opts;
  if (!addr.empty()) {
    opts.url = addr;
  }

  std::unique_ptr<metrics_sdk::PushMetricExporter> exporter{
      new metrics_exporter::PrometheusExporter(opts)};

  std::string version{METRIC_ZILLIQA_SCHEMA_VERSION};
  std::string schema{METRIC_ZILLIQA_SCHEMA};

  metrics_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
  options.export_timeout_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);
  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     options)};
  m_provider = std::shared_ptr<opentelemetry::metrics::MeterProvider>(
      new metrics_sdk::MeterProvider());
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(m_provider);
  p->AddMetricReader(std::move(reader));
  opentelemetry::metrics::Provider::SetMeterProvider(m_provider);
  zil::metrics::Filter::GetInstance().init();
}

void Metrics::Shutdown() {
  if (m_provider) {
    auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(m_provider);
    p->Shutdown();
  }
}

namespace {

inline auto GetMeter(
    std::shared_ptr<opentelemetry::metrics::MeterProvider>& provider,
    const std::string& family) {
  return provider->GetMeter(family, "1.2.0");
}

inline std::string GetFullName(const std::string& family,
                               const std::string& name) {
  std::string full_name;
  full_name.reserve(family.size() + name.size() + 1);
  full_name += family;
  full_name += "_";
  full_name += name;
  return full_name;
}

}  // namespace

zil::metrics::uint64Counter_t Metrics::CreateInt64Metric(
    const std::string& family, const std::string& name, const std::string& desc,
    std::string_view unit) {
  return GetMeter(m_provider, family)
      ->CreateUInt64Counter(GetFullName(family, name), desc, unit);
}

zil::metrics::doubleCounter_t Metrics::CreateDoubleMetric(
    const std::string& family, const std::string& name, const std::string& desc,
    std::string_view unit) {
  return GetMeter(m_provider, family)
      ->CreateDoubleCounter(GetFullName(family, name), desc, unit);
}

zil::metrics::Observable Metrics::CreateInt64UpDownMetric(
    zil::metrics::FilterClass filter, const std::string& family,
    const std::string& name, const std::string& desc, std::string_view unit) {
  return zil::metrics::Observable(
      filter, GetMeter(m_provider, family)
                  ->CreateInt64ObservableUpDownCounter(
                      GetFullName(family, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateDoubleUpDownMetric(
    zil::metrics::FilterClass filter, const std::string& family,
    const std::string& name, const std::string& desc, std::string_view unit) {
  return zil::metrics::Observable(
      filter, GetMeter(m_provider, family)
                  ->CreateDoubleObservableUpDownCounter(
                      GetFullName(family, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateInt64Gauge(
    zil::metrics::FilterClass filter, const std::string& family,
    const std::string& name, const std::string& desc, std::string_view unit) {
  return zil::metrics::Observable(
      filter,
      GetMeter(m_provider, family)
          ->CreateInt64ObservableGauge(GetFullName(family, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateDoubleGauge(
    zil::metrics::FilterClass filter, const std::string& family,
    const std::string& name, const std::string& desc, std::string_view unit) {
  return zil::metrics::Observable(
      filter,
      GetMeter(m_provider, family)
          ->CreateDoubleObservableGauge(GetFullName(family, name), desc, unit));
}

zil::metrics::doubleHistogram_t Metrics::CreateDoubleHistogram(
    const std::string& family, const std::string& name, const std::string& desc,
    std::string_view unit) {
  return GetMeter(m_provider, family)
      ->CreateDoubleHistogram(GetFullName(family, name), desc, unit);
}

zil::metrics::uint64Historgram_t Metrics::CreateUInt64Histogram(
    const std::string& family, const std::string& name, const std::string& desc,
    std::string_view unit) {
  return GetMeter(m_provider, family)
      ->CreateUInt64Histogram(GetFullName(family, name), desc, unit);
}

zil::metrics::Observable Metrics::CreateInt64ObservableCounter(
    zil::metrics::FilterClass filter, const std::string& family,
    const std::string& name, const std::string& desc, std::string_view unit) {
  return zil::metrics::Observable(
      filter, GetMeter(m_provider, family)
                  ->CreateInt64ObservableCounter(GetFullName(family, name),
                                                 desc, unit));
}

zil::metrics::Observable Metrics::CreateDoubleObservableCounter(
    zil::metrics::FilterClass filter, const std::string& family,
    const std::string& name, const std::string& desc, std::string_view unit) {
  return zil::metrics::Observable(
      filter, GetMeter(m_provider, family)
                  ->CreateDoubleObservableCounter(GetFullName(family, name),
                                                  desc, unit));
}

namespace zil::metrics {

namespace {

template <typename T>
using ObserverResult =
    std::shared_ptr<opentelemetry::v1::metrics::ObserverResultT<T>>;

template <typename T>
void SetT(opentelemetry::metrics::ObserverResult& result, T value,
          const common::KeyValueIterable& attributes) {
  bool holds_double = std::holds_alternative<ObserverResult<double>>(result);

  if constexpr (std::is_integral_v<T>) {
    assert(!holds_double);

    if (holds_double) {
      // ignore assert in release mode
      LOG_GENERAL(WARNING, "Integer metric expected");
      return;
    }
  } else {
    assert(holds_double);

    if (!holds_double) {
      // ignore assert in release mode
      LOG_GENERAL(WARNING, "Floating point metric expected");
      return;
    }
  }

  std::get<ObserverResult<T>>(result)->Observe(value, attributes);
}

}  // namespace

void Observable::Result::SetImpl(int64_t value,
                                 const common::KeyValueIterable& attributes) {
  SetT<int64_t>(m_result, value, attributes);
}

void Observable::Result::SetImpl(double value,
                                 const common::KeyValueIterable& attributes) {
  SetT<double>(m_result, value, attributes);
}

void Observable::SetCallback(Callback cb) {
  assert(cb);
  m_callback = std::move(cb);
  m_observable->AddCallback(&Observable::RawCallback, this);
}

Observable::~Observable() {
  if (m_callback) {
    m_observable->RemoveCallback(&Observable::RawCallback, this);
  }
}

void Observable::RawCallback(
    opentelemetry::metrics::ObserverResult observer_result, void* state) {
  assert(state);
  auto* self = static_cast<Observable*>(state);

  if (Filter::GetInstance().Enabled(self->m_filter)) {
    assert(self->m_callback);
    self->m_callback(Result(observer_result));
  }
}

namespace {

constexpr uint64_t ALL = std::numeric_limits<uint64_t>::max();

void UpdateMetricsMask(uint64_t& mask, const std::string& filter) {
  if (filter.empty()) {
    return;
  }

  if (filter == "ALL") {
    mask = ALL;
    return;
  }

#define CHECK_FILTER(FILTER)                              \
  if (filter == #FILTER) {                                \
    mask |= (1 << static_cast<int>(FilterClass::FILTER)); \
    return;                                               \
  }

  METRICS_FILTER_CLASSES(CHECK_FILTER)

#undef CHECK_FILTER
}

}  // namespace

void Filter::init() {
  std::vector<std::string> flags;
  boost::split(flags, METRIC_ZILLIQA_MASK, boost::is_any_of(","));
  for (const auto& f : flags) {
    UpdateMetricsMask(m_mask, f);
    if (m_mask == ALL) {
      break;
    }
  }
}

}  // namespace zil::metrics
