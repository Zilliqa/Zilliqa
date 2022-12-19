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

#include "Tracing.h"

#include <boost/algorithm/string.hpp>
//#include "opentelemetry/exporters/zipkin/zipkin_exporter_factory.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
//#include "opentelemetry/ext/zpages/zpages.h"  // Required file include for zpages
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

#include "common/TraceFilters.h"
#include "common/Constants.h"

namespace trace_api      = opentelemetry::trace;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace otlp           = opentelemetry::exporter::otlp;

#ifdef OTEL_1_81_1
namespace zipkin         = opentelemetry::exporter::zipkin;
#endif
namespace resource       = opentelemetry::sdk::resource;

Tracing::Tracing() { Init(); }

void
Tracing::Init(){
  std::string  cmp(TRACE_ZILLIQA_PROVIDER);

  if (cmp == "ZIPKIN"){
    ZipkinInit();
  } else if (cmp == "STDOUT") {
    StdOutInit();
  } else if (cmp == "ZPAGES"){
    ZPagesInit();
  }
  else if (cmp == "OTLP"){
    OtlpInit();
  }
  StdOutInit();
}


void
Tracing::OtlpInit(){
  opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
  // Create OTLP exporter instance
  auto exporter  = otlp::OtlpGrpcExporterFactory::Create(opts);
  auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  m_provider = trace_sdk::TracerProviderFactory::Create(std::move(processor));
  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(m_provider);
}

void
Tracing::ZipkinInit() {
#ifdef OTEL_1_81_1
  // Create zipkin exporter instance
  zipkin::ZipkinExporterOptions opts;
  resource::ResourceAttributes attributes = {{"service.name", "zipkin_demo_service"}};
  auto resource                           = resource::Resource::Create(attributes);
  auto exporter                           = zipkin::ZipkinExporterFactory::Create(opts);
  auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  m_provider = trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);
  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(m_provider);
#endif
}

void
Tracing::ZPagesInit() {
#ifdef OTEL_1_81_1
  ZPages::Initialize();
  m_provider = trace_api::Provider::GetTracerProvider();
  trace_api::Provider::SetTracerProvider(m_provider);
#endif
}

void Tracing::StdOutInit() {
  auto exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  m_provider = trace_sdk::TracerProviderFactory::Create(std::move(processor));

  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(m_provider);
}

std::shared_ptr<trace_api::Tracer> Tracing::get_tracer() {
  auto provider = trace_api::Provider::GetTracerProvider();
  return provider->GetTracer("zilliqa", OPENTELEMETRY_SDK_VERSION);
}

void Tracing::Shutdown() {
  if (m_provider) {
    std::shared_ptr<opentelemetry::trace::TracerProvider> provider(
        new opentelemetry::trace::NoopTracerProvider());

    // Set the global tracer provider
    trace_api::Provider::SetTracerProvider(provider);
  }
}

namespace zil::trace {
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

#define CHECK_FILTER(FILTER)                            \
  if (filter == #FILTER) {                                \
    mask |= (1 << static_cast<int>(FilterClass::FILTER)); \
    return;                                               \
  }

  TRACE_FILTER_CLASSES(CHECK_FILTER)
#undef CHECK_FILTER2
}
}

void Filter::init() {
  std::vector<std::string> flags;
  boost::split(flags, TRACE_ZILLIQA_MASK, boost::is_any_of(","));
  for (const auto& f : flags) {
    UpdateMetricsMask(m_mask, f);
    if (m_mask == ALL) {
      break;
    }
  }
}
}