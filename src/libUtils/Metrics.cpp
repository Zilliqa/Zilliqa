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
#include "Logger.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include <chrono>
#include <map>
#include <memory>
#include <thread>
#include <vector>

Metrics::Metrics(){
  Init();
}

void Metrics::Init(){

  try {
    unsigned int x;
    x = METRIC_ZILLIQA_PORT;
    x++;
  } catch (...){
    LOG_GENERAL(WARNING, "Failed to read Metrics specific setting in Constants.XML to disabling metrics");
    m_status = false;
    return;
  }


  std::string addr{std::string(METRIC_ZILLIQA_HOSTNAME)+":"+std::to_string(METRIC_ZILLIQA_PORT)};

  metrics_exporter::PrometheusExporterOptions opts;
  if (!addr.empty()) {
    opts.url = addr;
  }

  std::unique_ptr<metrics_sdk::PushMetricExporter> exporter{
      new metrics_exporter::PrometheusExporter(opts)};

  std::string version{METRIC_ZILLIQA_SCHEMA_VERSION};
  std::string schema{METRIC_ZILLIQA_SCHEMA};

  metrics_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis = std::chrono::milliseconds(METRIC_ZILLIQA_READER_EXPORT_MS);
  options.export_timeout_millis = std::chrono::milliseconds(METRIC_ZILLIQA_READER_TIMEOUT_MS);
  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter), options)};
  m_provider = std::shared_ptr<metrics_api::MeterProvider>(new metrics_sdk::MeterProvider());
  auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(m_provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(m_provider);
}

metrics::int64_t Metrics::CreateInt64Metric(const std::string& family,const std::string& name, const std::string& desc, std::string_view unit) {
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateUInt64Counter(family + "_" + name ,desc , unit);
}

metrics::double_t Metrics::CreateDoubleMetric(const std::string& family,const std::string& name, std::string_view unit) {
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateDoubleCounter(family + "_" + name  , unit);
}

metrics::int64Observable_t Metrics::CreateInt64UpDownMetric(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateInt64ObservableUpDownCounter(family + "_" + name, desc , unit);
}

metrics::doubleObservable_t Metrics::CreateDoubleUpDownMetric(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateDoubleObservableUpDownCounter(family + "_" + name, desc , unit);
}

metrics::int64Observable_t Metrics::CreateInt64Gauge(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateInt64ObservableUpDownCounter(family + "_" + name, desc , unit);
}

metrics::doubleObservable_t Metrics::CreateDoubleGauge(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateDoubleObservableUpDownCounter(family + "_" + name, desc , unit);
}

metrics::doubleHistogram_t Metrics::CreateDoubleHistogram(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateDoubleHistogram(family + "_" + name, desc , unit);
}

metrics::int64Historgram_t Metrics::CreateUInt64Histogram(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateUInt64Histogram(family + "_" + name, desc , unit);
}

metrics::int64Observable_t Metrics::CreateInt64ObservableCounter(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateInt64ObservableCounter(family + "_" + name, desc , unit);
}

metrics::doubleObservable_t Metrics::CreateDoubleObservableCounter(const std::string& family,const std::string& name,const std::string& desc, std::string_view unit){
  std::shared_ptr<metrics_api::Meter> meter = m_provider->GetMeter(family , "1.2.0");
  return meter->CreateDoubleObservableCounter(family + "_" + name, desc , unit);
}