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

#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/sdk/trace/tracer_context_factory.h>
#include <boost/algorithm/string.hpp>
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/propagation/b3_propagator.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"

#include "TraceFilters.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace otlp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;

Tracing::Tracing() { Init(); }

void Tracing::Init() {
  if (not TRACE_ZILLIQA_MASK.empty() and TRACE_ZILLIQA_MASK != "NONE") {
    zil::trace::Filter::GetInstance().init();
  }
  std::string cmp{TRACE_ZILLIQA_PROVIDER};

  if (cmp == "OTLPHTTP") {
    OtlpHTTPInit();
  } else if (cmp == "STDOUT") {
    StdOutInit();
  } else {
    LOG_GENERAL(WARNING,
                "Telemetry provider has defaulted to NOOP provider due to no "
                "configuration");
    NoopInit();
  }
}

void Tracing::NoopInit() {
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider(
      new opentelemetry::trace::NoopTracerProvider());

  // Set the global tracer provider
  trace_api::Provider::SetTracerProvider(provider);

  LOG_GENERAL(INFO, "Trace set to NOOP Provider");
}

void Tracing::OtlpHTTPInit() {
  opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
  std::string addr{std::string(TRACE_ZILLIQA_HOSTNAME) + ":" +
                   boost::lexical_cast<std::string>(TRACE_ZILLIQA_PORT)};
  if (!addr.empty()) {
    opts.url = "http://" + addr + "/v1/traces";
  }
  resource::ResourceAttributes attributes = {{"service.name", "zilliqa-cpp"},
                                             {"version", (uint32_t)1}};

  auto resource = resource::Resource::Create(attributes);
  // Create OTLP exporter instance
  auto exporter = otlp::OtlpHttpExporterFactory::Create(opts);
  auto processor =
      opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
          std::move(exporter));
  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
      processors;
  processors.push_back(std::move(processor));
  // Default is an always-on sampler.
  std::shared_ptr<opentelemetry::sdk::trace::TracerContext> context =
      opentelemetry::sdk::trace::TracerContextFactory::Create(
          std::move(processors));

  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(context);

  trace_api::Provider::SetTracerProvider(provider);

  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

void Tracing::InitOtlpGrpc() {
  opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
  auto exporter = otlp::OtlpGrpcExporterFactory::Create(opts);
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor));
  // Set the global trace provider
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

void Tracing::StdOutInit() {
  auto exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  resource::ResourceAttributes attributes = {{"service.name", "zilliqa-cpp"},
                                             {"version", (uint32_t)1}};
  auto resource = resource::Resource::Create(attributes);
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);

  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(provider);

  // Setup a prpogator

  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

std::shared_ptr<trace_api::Tracer> Tracing::get_tracer() {
  return trace_api::Provider::GetTracerProvider()->GetTracer(
      "zilliqa-cpp", OPENTELEMETRY_SDK_VERSION);
}

void Tracing::Shutdown() {
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider(
      new opentelemetry::trace::NoopTracerProvider());

  // Set the global tracer provider
  trace_api::Provider::SetTracerProvider(provider);
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

#define CHECK_FILTER(FILTER)                              \
  if (filter == #FILTER) {                                \
    mask |= (1 << static_cast<int>(FilterClass::FILTER)); \
    return;                                               \
  }

  TRACE_FILTER_CLASSES(CHECK_FILTER)
#undef CHECK_FILTER2
}
}  // namespace

void Filter::init(const std::string& arg) {
  const std::string& mask_str = arg.empty() ? TRACE_ZILLIQA_MASK : arg;
  std::vector<std::string> flags;
  boost::split(flags, mask_str, boost::is_any_of(","));
  for (const auto& f : flags) {
    UpdateMetricsMask(m_mask, f);
    if (m_mask == ALL) {
      break;
    }
  }
}

namespace {

constexpr size_t FLAGS_OFFSET = 0;
constexpr size_t FLAGS_SIZE = 2;
constexpr size_t SPAN_ID_OFFSET = FLAGS_SIZE + 1;
constexpr size_t SPAN_ID_SIZE = 16;
constexpr size_t TRACE_ID_OFFSET = SPAN_ID_OFFSET + SPAN_ID_SIZE + 1;
constexpr size_t TRACE_ID_SIZE = 32;
constexpr size_t TRACE_INFO_SIZE =
    FLAGS_SIZE + 1 + SPAN_ID_SIZE + 1 + TRACE_ID_SIZE;

trace_api::SpanContext ExtractSpanContextFromTraceInfo(
    const std::string& traceInfo) {
  if (traceInfo.size() != TRACE_INFO_SIZE) {
    LOG_GENERAL(WARNING, "Unexpected trace info size " << traceInfo.size());
    return trace_api::SpanContext::GetInvalid();
  }

  if (traceInfo[SPAN_ID_OFFSET - 1] != '-' ||
      traceInfo[TRACE_ID_OFFSET - 1] != '-') {
    LOG_GENERAL(WARNING, "Invalid format of trace info " << traceInfo);
    return trace_api::SpanContext::GetInvalid();
  }

  std::string_view trace_id_hex(traceInfo.data() + TRACE_ID_OFFSET,
                                TRACE_ID_SIZE);
  std::string_view span_id_hex(traceInfo.data() + SPAN_ID_OFFSET, SPAN_ID_SIZE);
  std::string_view trace_flags_hex(traceInfo.data() + FLAGS_OFFSET, FLAGS_SIZE);

  using trace_api::propagation::detail::IsValidHex;

  if (!IsValidHex(trace_id_hex) || !IsValidHex(span_id_hex) ||
      !IsValidHex(trace_flags_hex)) {
    LOG_GENERAL(WARNING, "Invalid hex of trace info fields: " << traceInfo);
    return trace_api::SpanContext::GetInvalid();
  }

  using trace_api::propagation::B3PropagatorExtractor;

  auto trace_id = B3PropagatorExtractor::TraceIdFromHex(trace_id_hex);
  auto span_id = B3PropagatorExtractor::SpanIdFromHex(span_id_hex);
  auto trace_flags = B3PropagatorExtractor::TraceFlagsFromHex(trace_flags_hex);

  if (!trace_id.IsValid() || !span_id.IsValid()) {
    LOG_GENERAL(WARNING, "Invalid trace_id or span_id in " << traceInfo);
    return trace_api::SpanContext::GetInvalid();
  }

  return trace_api::SpanContext(trace_id, span_id, trace_flags, true);
}

}  // namespace

void ExtractTraceInfoFromActiveSpan(std::string& out) {
  auto activeSpan = Tracing::GetInstance().get_tracer()->GetCurrentSpan();
  auto spanContext = activeSpan->GetContext();
  if (!spanContext.IsValid()) {
    LOG_GENERAL(WARNING, "No active spans");
    out.clear();
    return;
  }

  out.assign(TRACE_INFO_SIZE, '-');
  spanContext.trace_flags().ToLowerBase16(
      std::span<char, FLAGS_SIZE>(out.data() + FLAGS_OFFSET, FLAGS_SIZE));
  spanContext.span_id().ToLowerBase16(
      std::span<char, SPAN_ID_SIZE>(out.data() + SPAN_ID_OFFSET, SPAN_ID_SIZE));
  spanContext.trace_id().ToLowerBase16(std::span<char, TRACE_ID_SIZE>(
      out.data() + TRACE_ID_OFFSET, TRACE_ID_SIZE));
}

std::shared_ptr<trace_api::Span> CreateChildSpan(
    std::string_view name, const std::string& serializedTraceInfo) {
  auto spanCtx = ExtractSpanContextFromTraceInfo(serializedTraceInfo);
  if (!spanCtx.IsValid()) {
    return trace_api::Tracer::GetCurrentSpan();
  }

  trace_api::StartSpanOptions options;

  // child spans from deserialized parent  are of server kind
  options.kind = trace_api::SpanKind::kServer;
  options.parent = spanCtx;

  return Tracing::GetInstance().get_tracer()->StartSpan(name, options);
}

}  // namespace zil::trace
