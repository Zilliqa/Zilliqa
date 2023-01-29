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

#include "Metrics.h"

#include <vector>

#include <boost/algorithm/string.hpp>

#include <opentelemetry/sdk/metrics/view/view.h>
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/resource/resource.h"

#include "Tracing.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace metrics_exporter = opentelemetry::exporter::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace otlp_exporter = opentelemetry::exporter::otlp;

// The OpenTelemetry Metrics Interface.

Metrics::Metrics() { Init(); }

namespace {
const double METRICS_VERSION{8.6};
const std::string ZILLIQA_METRIC_FAMILY{"zilliqa-cpp"};
}  // namespace

void Metrics::Init() {
  zil::metrics::Filter::GetInstance().init();

  std::string cmp(METRIC_ZILLIQA_PROVIDER);

  if (cmp == "PROMETHEUS") {
    InitPrometheus(METRIC_ZILLIQA_HOSTNAME + ":" +
                   std::to_string(METRIC_ZILLIQA_PORT));

  } else if (cmp == "OTLPHTTP") {
    InitOTHTTP();
  } else if (cmp == "OTLPGRPC") {
    InitOtlpGrpc();
  } else {
    InitStdOut();  // our favourite
  }
}

// TODO:: could probably optimise out the span with getcurrent span
// Capture EMT - multipurpose talk to all capture event metric , log , trace of
// event next step add linkage.

bool Metrics::CaptureEMT(std::shared_ptr<opentelemetry::trace::Span> &span,
                         zil::metrics::FilterClass fc,
                         zil::trace::FilterClass tc,
                         zil::metrics::uint64Counter_t &metric,
                         const std::string &messageText, const uint8_t &code) {
  try {
    if (not messageText.empty()) {
      LOG_GENERAL(WARNING, messageText);
    }
    if (zil::trace::Filter::GetInstance().Enabled(tc)) {
      span->SetStatus(opentelemetry::trace::StatusCode::kError, messageText);
    }
    if (zil::metrics::Filter::GetInstance().Enabled(fc) &&
        metric.get() != nullptr) {
      metric->Add(1, {{"error", __FUNCTION__}});
    }
  } catch (...) {
    return false;
  }
  return true;
}

void Metrics::InitStdOut() {
  std::unique_ptr<metrics_sdk::PushMetricExporter> exporter{
      new metrics_exporter::OStreamMetricExporter};
  // Initialize and set the global MeterProvider
  metrics_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
  options.export_timeout_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);

  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     options)};

  opentelemetry::sdk::resource::ResourceAttributes attributes = {
      {"service.name", "zilliqa-daemon"}, {"version", (double)METRICS_VERSION}};
  auto resource = opentelemetry::sdk::resource::Resource::Create(attributes);
  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metrics_sdk::MeterProvider(
          std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry>(
              new opentelemetry::sdk::metrics::ViewRegistry()),
          resource));
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(provider);

  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(p);
}

void Metrics::InitOTHTTP() {
  metrics_sdk::PeriodicExportingMetricReaderOptions opts;
  std::string addr{std::string(METRIC_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(METRIC_ZILLIQA_PORT)};

  otlp_exporter::OtlpHttpMetricExporterOptions options;
  if (!addr.empty()) {
    options.url = "http://" + addr + "/v1/metrics";
    options.console_debug = true;
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
    options.aggregation_temporality =
        opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  }
  std::unique_ptr<metrics_sdk::PushMetricExporter> exporter =
      otlp_exporter::OtlpHttpMetricExporterFactory::Create(options);

  opentelemetry::sdk::resource::ResourceAttributes attributes = {
      {"service.name", "zilliqa-daemon"}, {"version", (double)METRICS_VERSION}};
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      attributes, METRIC_ZILLIQA_SCHEMA);

  opts.export_interval_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
  opts.export_timeout_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);
  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     opts)};
  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metrics_sdk::MeterProvider(
          std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry>(
              new opentelemetry::sdk::metrics::ViewRegistry()),
          resource));
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(p);
}

void Metrics::InitOtlpGrpc() {
  otlp_exporter::OtlpGrpcMetricExporterOptions options;
  metrics_sdk::PeriodicExportingMetricReaderOptions opts;
  std::string addr{std::string(METRIC_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(METRIC_ZILLIQA_PORT)};

  opentelemetry::sdk::resource::ResourceAttributes attributes = {
      {"service.name", "zilliqa-daemon"}, {"version", (double)METRICS_VERSION}};
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      attributes, METRIC_ZILLIQA_SCHEMA);

  opts.export_interval_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
  opts.export_timeout_millis =
      std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);

  options.endpoint = addr;
  options.aggregation_temporality =
      opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;

  auto exporter = otlp_exporter::OtlpGrpcMetricExporterFactory::Create(options);
  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     opts)};
  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metrics_sdk::MeterProvider(
          std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry>(
              new opentelemetry::sdk::metrics::ViewRegistry()),
          resource));

  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(p);
}

void Metrics::InitPrometheus(
    const std::string &addr) {  // To be Deprecated in Otel API
  metrics_exporter::PrometheusExporterOptions opts;
  if (!addr.empty()) {
    opts.url = addr;
  }
  std::unique_ptr<metrics_sdk::PushMetricExporter> exporter{
      new metrics_exporter::PrometheusExporter(opts)};

  metrics_sdk::PeriodicExportingMetricReaderOptions options;

  opentelemetry::sdk::resource::ResourceAttributes attributes = {
      {"service.name", "zilliqa-daemon"}, {"version", (double)METRICS_VERSION}};
  auto resource = opentelemetry::sdk::resource::Resource::Create(attributes);

  options.export_interval_millis = std::chrono::milliseconds(1000);
  options.export_timeout_millis = std::chrono::milliseconds(500);
  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     options)};

  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metrics_sdk::MeterProvider(
          std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry>(
              new opentelemetry::sdk::metrics::ViewRegistry()),
          resource));
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(provider);

  p->AddMetricReader(std::move(reader));

  metrics_api::Provider::SetMeterProvider(provider);
}

void Metrics::Shutdown() {
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(
      metrics_api::Provider::GetMeterProvider());
  p->Shutdown();
}

namespace {

inline auto GetMeter(
    std::shared_ptr<opentelemetry::metrics::MeterProvider> &provider,
    const std::string &family) {
  return provider->GetMeter(family, "1.2.0", METRIC_ZILLIQA_SCHEMA);
}

inline std::string GetFullName(const std::string &family,
                               const std::string &name) {
  std::string full_name;
  full_name.reserve(family.size() + name.size() + 1);
  full_name += family;
  full_name += "_";
  full_name += name;
  return full_name;
}

}  // namespace

zil::metrics::uint64Counter_t Metrics::CreateInt64Metric(
    const std::string &name, const std::string &desc, std::string unit) {
  return GetMeter()->CreateUInt64Counter(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit);
}

zil::metrics::doubleCounter_t Metrics::CreateDoubleMetric(
    const std::string &name, const std::string &desc, std::string unit) {
  return GetMeter()->CreateDoubleCounter(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit);
}

zil::metrics::doubleHistogram_t Metrics::CreateDoubleHistogram(
    const std::string &name, const std::string &desc, std::string unit) {
  return GetMeter()->CreateDoubleHistogram(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit);
}

zil::metrics::Observable Metrics::CreateInt64UpDownMetric(
    const std::string &name, const std::string &desc, std::string unit) {
  return zil::metrics::Observable(
      GetMeter()->CreateInt64ObservableUpDownCounter(
          GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateDoubleUpDownMetric(
    const std::string &name, const std::string &desc, std::string unit) {
  return zil::metrics::Observable(
      GetMeter()->CreateDoubleObservableUpDownCounter(
          GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateInt64Gauge(const std::string &name,
                                                   const std::string &desc,
                                                   std::string unit) {
  return zil::metrics::Observable(GetMeter()->CreateInt64ObservableGauge(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateDoubleGauge(const std::string &name,
                                                    const std::string &desc,
                                                    std::string unit) {
  return zil::metrics::Observable(GetMeter()->CreateDoubleObservableGauge(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateInt64ObservableCounter(
    const std::string &name, const std::string &desc, std::string unit) {
  return zil::metrics::Observable(GetMeter()->CreateInt64ObservableCounter(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit));
}

zil::metrics::Observable Metrics::CreateDoubleObservableCounter(
    const std::string &name, const std::string &desc, std::string unit) {
  return zil::metrics::Observable(GetMeter()->CreateDoubleObservableCounter(
      GetFullName(ZILLIQA_METRIC_FAMILY, name), desc, unit));
}

void Metrics::AddCounterSumView(const std::string &name,
                                const std::string &description) {
  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(
      metrics_api::Provider::GetMeterProvider());
  std::shared_ptr<opentelemetry::metrics::Meter> meter =
      p->GetMeter("zilliqa", "1.2.0");
  // counter view
  std::string counter_name = name + "_counter";
  std::unique_ptr<metrics_sdk::InstrumentSelector> instrument_selector{
      new metrics_sdk::InstrumentSelector(metrics_sdk::InstrumentType::kCounter,
                                          counter_name)};
  std::unique_ptr<metrics_sdk::MeterSelector> meter_selector{
      new metrics_sdk::MeterSelector(name, version, schema)};
  std::unique_ptr<metrics_sdk::View> sum_view{new metrics_sdk::View{
      name, description, metrics_sdk::AggregationType::kSum}};
  p->AddView(std::move(instrument_selector), std::move(meter_selector),
             std::move(sum_view));
}

void Metrics::AddCounterHistogramView(const std::string name,
                                      std::list<double> list,
                                      const std::string &description) {
  // counter view

  std::unique_ptr<metrics_sdk::InstrumentSelector>
      histogram_instrument_selector{new metrics_sdk::InstrumentSelector(
          metrics_sdk::InstrumentType::kHistogram, name)};

  std::unique_ptr<metrics_sdk::MeterSelector> histogram_meter_selector{
      new metrics_sdk::MeterSelector(ZILLIQA_METRIC_FAMILY,
                                     METRIC_ZILLIQA_SCHEMA_VERSION,
                                     METRIC_ZILLIQA_SCHEMA)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig>
      aggregation_config{
          new opentelemetry::sdk::metrics::HistogramAggregationConfig};

  static_cast<opentelemetry::sdk::metrics::HistogramAggregationConfig *>(
      aggregation_config.get())
      ->boundaries_ = list;

  std::unique_ptr<metrics_sdk::View> histogram_view{new metrics_sdk::View{
      name,
      description,
      metrics_sdk::AggregationType::kHistogram,
      aggregation_config,
  }};

  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(
      metrics_api::Provider::GetMeterProvider());
  p->AddView(std::move(histogram_instrument_selector),
             std::move(histogram_meter_selector), std::move(histogram_view));
}

std::shared_ptr<opentelemetry::metrics::Meter> Metrics::GetMeter() {
  const auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(
      metrics_api::Provider::GetMeterProvider());
  return p->GetMeter(ZILLIQA_METRIC_FAMILY, METRIC_ZILLIQA_SCHEMA_VERSION,
                     METRIC_ZILLIQA_SCHEMA);
}

namespace zil::metrics {

namespace {

template <typename T>
using ObserverResult =
    std::shared_ptr<opentelemetry::v1::metrics::ObserverResultT<T>>;

template <typename T>
void SetT(opentelemetry::metrics::ObserverResult &result, T value,
          const common::KeyValueIterable &attributes) {
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
                                 const common::KeyValueIterable &attributes) {
  SetT<int64_t>(m_result, value, attributes);
}

void Observable::Result::SetImpl(double value,
                                 const common::KeyValueIterable &attributes) {
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
    opentelemetry::metrics::ObserverResult observer_result, void *state) {
  assert(state);
  auto *self = static_cast<Observable *>(state);

  assert(self->m_callback);
  self->m_callback(Result(observer_result));
}

namespace {

constexpr uint64_t ALL = std::numeric_limits<uint64_t>::max();

void UpdateMetricsMask(uint64_t &mask, const std::string &filter) {
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
  for (const auto &f : flags) {
    UpdateMetricsMask(m_mask, f);
    if (m_mask == ALL) {
      break;
    }
  }
}

}  // namespace zil::metrics
