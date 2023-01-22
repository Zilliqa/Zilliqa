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
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"

#include "common/Constants.h"
#include "common/TraceFilters.h"

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace otlp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;

Tracing::Tracing() { Init(); }

void Tracing::Init() {
  std::string cmp{TRACE_ZILLIQA_PROVIDER};

  if (cmp == "OTLPHTTP") {
      OtlpHTTPInit();
  } else {
      StdOutInit();
  }
}

void Tracing::OtlpHTTPInit() {
  opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
  std::string addr{std::string(TRACE_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(TRACE_ZILLIQA_PORT)};
  if (!addr.empty()) {
      opts.url = "http://" + addr + "/v1/traces";
  }
  resource::ResourceAttributes attributes = {{"service.name", "zilliqa-cpp"},
                                             {"version", (uint32_t)1}};
  auto resource = resource::Resource::Create(attributes);
  // Create OTLP exporter instance
  auto exporter = otlp::OtlpHttpExporterFactory::Create(opts);
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  m_provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);
  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(m_provider);

  // Setup a prpogator

  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

void Tracing::StdOutInit() {
  auto exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  resource::ResourceAttributes attributes = {{"service.name", "zilliqa-cpp"},
                                             {"version", (uint32_t)1}};
  auto resource = resource::Resource::Create(attributes);

  m_provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);

  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(m_provider);

  // Setup a prpogator

  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

std::shared_ptr<trace_api::Tracer> Tracing::get_tracer() {
  return m_provider->GetTracer("zilliqa-cpp", OPENTELEMETRY_SDK_VERSION);
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

#define CHECK_FILTER(FILTER)                              \
  if (filter == #FILTER) {                                \
    mask |= (1 << static_cast<int>(FilterClass::FILTER)); \
    return;                                               \
  }

  TRACE_FILTER_CLASSES(CHECK_FILTER)
#undef CHECK_FILTER2
}
}  // namespace

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

namespace {

struct TextMapCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override {
    if (IsTraceParent(key)) {
      return m_traceParent;
    } else if (IsTraceState(key)) {
      return m_traceState;
    }
    std::cout << "Ignoring " << key << std::endl;
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    if (IsTraceParent(key)) {
      m_traceParent = value;
    } else if (IsTraceState(key)) {
      m_traceState = value;
    } else {
        std::cout << "Ignoring " << key << " = " << value;
    }
  }

  void Serialize(std::string& out) {
    if (!m_traceState.empty()) {
      m_traceParent.reserve(m_traceParent.size() + 1 + m_traceState.size());
      m_traceParent += DELIMITER;
      m_traceParent += m_traceState;
    }
    out = std::move(m_traceParent);
  }

  void Deserialize(const std::string& str) {
    auto pos = str.find(DELIMITER);
    if (pos != std::string::npos) {
      m_traceParent = str.substr(0, pos);
      m_traceState = str.substr(pos + 1);
    } else {
      m_traceParent = str;
    }
  }

 private:
  static constexpr char DELIMITER = ':';

  static bool IsTraceParent(opentelemetry::nostd::string_view key) {
    return (key == opentelemetry::trace::propagation::kTraceParent ||
            key == "Traceparent");
  }

  static bool IsTraceState(opentelemetry::nostd::string_view key) {
    return (key == opentelemetry::trace::propagation::kTraceState ||
            key == "Tracestate");
  }

  std::string m_traceParent;
  std::string m_traceState;
};

}  // namespace

void ExtractTraceInfoFromCurrentContext(std::string& out) {
  auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  TextMapCarrier carrier;
  auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::
      GetGlobalPropagator();
  prop->Inject(carrier, current_ctx);
}

std::shared_ptr<trace_api::Span> CreateChildSpan(
    std::string_view name, const std::string& serializedTraceInfo) {
  auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::
      GetGlobalPropagator();
  TextMapCarrier carrier;
  carrier.Deserialize(serializedTraceInfo);

  auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  auto new_context = prop->Extract(carrier, current_ctx);

  trace_api::StartSpanOptions options;

  // child spans from deserialized parent  are of server kind
  options.kind = trace_api::SpanKind::kServer;
  options.parent = trace_api::GetSpan(new_context)->GetContext();

  return Tracing::GetInstance().get_tracer()->StartSpan(name, options);
}

}  // namespace zil::trace
