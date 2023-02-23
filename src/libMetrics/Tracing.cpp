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

#include "Tracing.h"

#include <cassert>
#include <thread>

#include <boost/algorithm/string.hpp>

#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_context_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/propagation/b3_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"

#include "libUtils/Logger.h"

namespace zil::trace {

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace otlp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;
namespace otel_std = opentelemetry::v1::nostd;

static_assert(std::is_same_v<otel_std::shared_ptr<int>, std::shared_ptr<int>>);
static_assert(std::is_same_v<otel_std::unique_ptr<int>, std::unique_ptr<int>>);

namespace {

inline opentelemetry::common::AttributeValue ToInternal(Value v) {
  return std::visit(
      [](auto&& v1) {
        return opentelemetry::common::AttributeValue(std::move(v1));
      },
      v);
}

void GetIdsImpl(std::string& out, const trace_api::SpanContext& spanContext,
                const std::string& identity);

std::optional<trace_api::SpanContext> ExtractSpanContextFromIds(
    std::string_view serializedIds);

std::string_view ExtractSenderIdentityFromIds(std::string_view serializedIds);

std::optional<std::pair<std::string_view, std::string_view>> ExtractStringIds(
    std::string_view serializedIds);

template <typename Container>
class KVI : public opentelemetry::common::KeyValueIterable {
  Container& m_cont;

 public:
  KVI(Container& c) : m_cont(c) {}

  bool ForEachKeyValue(
      otel_std::function_ref<bool(otel_std::string_view,
                                  opentelemetry::common::AttributeValue)>
          callback) const noexcept override {
    for (auto& pair : m_cont) {
      if (!callback(pair.first, ToInternal(std::move(pair.second)))) {
        return false;
      }
    }
    return true;
  }

  size_t size() const noexcept override { return m_cont.size(); }
};

}  // namespace

class TracingImpl {
  // thread local stack of spans
  class Stack {
    std::vector<std::shared_ptr<Span::Impl>> m_stack;

   public:
    static Stack& GetInstance() {
      static thread_local Stack stack;
      return stack;
    }

    bool Empty() const { return m_stack.empty(); }

    const std::shared_ptr<Span::Impl>& GetActiveSpan() const {
      static const std::shared_ptr<Span::Impl> emptySpan;
      return m_stack.empty() ? emptySpan : m_stack.back();
    }

    void Push(std::shared_ptr<Span::Impl> span) {
      assert(span);
      assert(span->IsRecording());
      m_stack.emplace_back(std::move(span));
    }

    void Pop() {
      assert(!m_stack.empty());
      m_stack.pop_back();
    }
  };

  // wrapper
  class SpanImpl : public Span::Impl {
    // internal span impl
    otel_std::shared_ptr<trace_api::Span> m_span;

    // token of active scope
    otel_std::unique_ptr<opentelemetry::context::Token> m_token;

    // thread id saved to prevent inter-thread violations of scopes by the
    // calling code
    std::thread::id m_threadId;

    // let it be here, because their GetContext() moves too many bytes every
    // call
    trace_api::SpanContext m_context;

    // serialized span identity
    std::string m_ids;

    bool IsRecording() const noexcept override { return m_span->IsRecording(); }

    SpanId GetSpanId() const noexcept override { return m_context.span_id(); }

    TraceId GetTraceId() const noexcept override {
      return m_context.trace_id();
    }

    const std::string& GetIds() const noexcept override { return m_ids; }

    void AddEvent(std::string_view name,
                  std::initializer_list<std::pair<std::string_view, Value>>
                      attributes) noexcept override {
      KVI iterable(attributes);
      m_span->AddEvent(name, iterable);
    }

    void End(StatusCode status = StatusCode::UNSET) noexcept override {
      if (m_token) {
        if (m_threadId != std::this_thread::get_id()) {
          LOG_GENERAL(FATAL, "Tracing scope usage violation (threading)");
          abort();
        }

        m_span->SetStatus(static_cast<trace_api::StatusCode>(status));
        m_span->End();
        m_token.reset();
        Stack::GetInstance().Pop();
      }
    }

   public:
    SpanImpl(otel_std::shared_ptr<trace_api::Span> span,
             otel_std::unique_ptr<opentelemetry::context::Token> token,
             const std::string& identity)
        : m_span(std::move(span)),
          m_token(std::move(token)),
          m_threadId(std::this_thread::get_id()),
          m_context(m_span->GetContext()) {
      assert(m_token);
      assert(m_context.IsValid());
      GetIdsImpl(m_ids, m_context, identity);
    }

    void SetAttribute(std::string_view name, Value value) noexcept override {
      m_span->SetAttribute(name, ToInternal(std::move(value)));
    }
  };

  // Filters mask. Can be zero if tracing is not enabled or initialized
  uint64_t m_filtersMask{};

  // This node's identity, A.K.A service.name attribute value
  std::string m_identity;

  // Tracer which creates spans. Can be nullptr if tracing is not enabled or
  // initialized
  otel_std::shared_ptr<trace_api::Tracer> m_tracer;

  Span CreateSpanImpl(std::string_view name,
                      const trace_api::StartSpanOptions& options,
                      std::string_view remote_node_identity) {
    assert(m_tracer);

    auto internalSpan = m_tracer->StartSpan(name, options);
    assert(internalSpan);

    auto token = opentelemetry::context::RuntimeContext::Attach(
        opentelemetry::context::RuntimeContext::GetCurrent().SetValue(
            trace_api::kSpanKey,
            opentelemetry::context::ContextValue(internalSpan)));
    auto impl = std::make_shared<SpanImpl>(std::move(internalSpan),
                                           std::move(token), m_identity);
    if (!remote_node_identity.empty()) {
      impl->SetAttribute("dtrace.from", remote_node_identity);
    }
    Stack::GetInstance().Push(impl);
    return Span(std::move(impl), true);
  }

 public:
  static bool HasActiveSpan() {
    // thread local instance here
    return !Stack::GetInstance().Empty();
  }

  static Span GetActiveSpan() {
    // thread local instance here
    auto& stack = Stack::GetInstance();
    if (stack.Empty()) {
      return Span{};
    }
    return Span(stack.GetActiveSpan(), false);
  }

  static std::optional<std::pair<TraceId, SpanId>> GetActiveSpanIds() {
    // thread local instance here
    auto& stack = Stack::GetInstance();
    if (stack.Empty()) {
      return std::nullopt;
    }
    const auto& span = stack.GetActiveSpan();
    return std::pair(span->GetTraceId(), span->GetSpanId());
  }

  static std::optional<std::pair<std::string_view, std::string_view>>
  GetActiveSpanStringIds() {
    // thread local instance here
    auto& stack = Stack::GetInstance();
    if (stack.Empty()) {
      return std::nullopt;
    }
    const auto& span = stack.GetActiveSpan();
    return ExtractStringIds(span->GetIds());
  }

  static TracingImpl& GetInstance() {
    static TracingImpl tracing;
    return tracing;
  }

  bool Initialize(std::string_view global_name, std::string_view filters_mask);

  bool IsEnabled(FilterClass to_test) const {
    return m_filtersMask & (1 << static_cast<int>(to_test));
  }

  bool IsEnabled() const { return m_filtersMask != 0; }

  Span CreateSpan(FilterClass filter, std::string_view name) {
    if (m_tracer && IsEnabled(filter)) {
      trace_api::StartSpanOptions options;
      return CreateSpanImpl(name, options, "");
    }
    return Span{};
  }

  Span CreateChildSpanOfRemoteTrace(FilterClass filter, std::string_view name,
                                    std::string_view remote_trace_info) {
    if (m_tracer && IsEnabled(filter)) {
      auto ctx_opt = ExtractSpanContextFromIds(remote_trace_info);
      if (!ctx_opt.has_value()) {
        return Span{};
      }

      trace_api::StartSpanOptions options;

      // child spans from deserialized parent  are of server kind
      options.kind = trace_api::SpanKind::kServer;
      options.parent = std::move(ctx_opt.value());

      auto span = CreateSpanImpl(
          name, options, ExtractSenderIdentityFromIds(remote_trace_info));
    }
    return Span{};
  }

  TracingImpl() = default;
};

bool Tracing::Initialize(std::string_view identity,
                         std::string_view filters_mask) {
  static std::once_flag initialized;
  bool result = false;
  std::call_once(initialized, [&result, &identity, &filters_mask] {
    result = TracingImpl::GetInstance().Initialize(identity, filters_mask);
  });
  return result;
}

bool Tracing::IsEnabled(FilterClass filter) {
  return TracingImpl::GetInstance().IsEnabled(filter);
}

bool Tracing::IsEnabled() { return TracingImpl::GetInstance().IsEnabled(); }

Span Tracing::CreateSpan(FilterClass filter, std::string_view name) {
  return TracingImpl::GetInstance().CreateSpan(filter, name);
}

Span Tracing::CreateChildSpanOfRemoteTrace(FilterClass filter,
                                           std::string_view name,
                                           std::string_view remote_trace_info) {
  if (remote_trace_info.empty()) {
    return Span{};
  }
  return TracingImpl::GetInstance().CreateChildSpanOfRemoteTrace(
      filter, name, remote_trace_info);
}

bool Tracing::HasActiveSpan() { return TracingImpl::HasActiveSpan(); }

Span Tracing::GetActiveSpan() { return TracingImpl::GetActiveSpan(); }

std::optional<std::pair<TraceId, SpanId>> Tracing::GetActiveSpanIds() {
  return TracingImpl::GetActiveSpanIds();
}

std::optional<std::pair<std::string_view, std::string_view>>
Tracing::GetActiveSpanStringIds() {
  return TracingImpl::GetActiveSpanStringIds();
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

void GetIdsImpl(std::string& out, const trace_api::SpanContext& spanContext,
                const std::string& identity) {
  out.reserve(TRACE_INFO_SIZE + 1 + identity.size());
  out.assign(TRACE_INFO_SIZE + 1, '-');
  spanContext.trace_flags().ToLowerBase16(
      std::span<char, FLAGS_SIZE>(out.data() + FLAGS_OFFSET, FLAGS_SIZE));
  spanContext.span_id().ToLowerBase16(
      std::span<char, SPAN_ID_SIZE>(out.data() + SPAN_ID_OFFSET, SPAN_ID_SIZE));
  spanContext.trace_id().ToLowerBase16(std::span<char, TRACE_ID_SIZE>(
      out.data() + TRACE_ID_OFFSET, TRACE_ID_SIZE));
  out += identity;
}

std::optional<trace_api::SpanContext> ExtractSpanContextFromIds(
    std::string_view serializedIds) {
  if (serializedIds.size() < TRACE_INFO_SIZE + 1) {
    LOG_GENERAL(WARNING, "Unexpected trace info size " << serializedIds.size());
    return std::nullopt;
  }

  if (serializedIds[SPAN_ID_OFFSET - 1] != '-' ||
      serializedIds[TRACE_ID_OFFSET - 1] != '-' ||
      serializedIds[TRACE_INFO_SIZE] != '-') {
    LOG_GENERAL(WARNING, "Invalid format of trace info " << serializedIds);
    return std::nullopt;
  }

  std::string_view trace_id_hex(serializedIds.data() + TRACE_ID_OFFSET,
                                TRACE_ID_SIZE);
  std::string_view span_id_hex(serializedIds.data() + SPAN_ID_OFFSET,
                               SPAN_ID_SIZE);
  std::string_view trace_flags_hex(serializedIds.data() + FLAGS_OFFSET,
                                   FLAGS_SIZE);

  using trace_api::propagation::detail::IsValidHex;

  if (!IsValidHex(trace_id_hex) || !IsValidHex(span_id_hex) ||
      !IsValidHex(trace_flags_hex)) {
    LOG_GENERAL(WARNING, "Invalid hex of trace info fields: " << serializedIds);
    return std::nullopt;
  }

  using trace_api::propagation::B3PropagatorExtractor;

  auto trace_id = B3PropagatorExtractor::TraceIdFromHex(trace_id_hex);
  auto span_id = B3PropagatorExtractor::SpanIdFromHex(span_id_hex);
  auto trace_flags = B3PropagatorExtractor::TraceFlagsFromHex(trace_flags_hex);

  if (!trace_id.IsValid() || !span_id.IsValid()) {
    LOG_GENERAL(WARNING, "Invalid trace_id or span_id in " << serializedIds);
    return std::nullopt;
  }

  return trace_api::SpanContext(trace_id, span_id, trace_flags, true);
}

std::string_view ExtractSenderIdentityFromIds(std::string_view serializedIds) {
  return (serializedIds.size() > TRACE_INFO_SIZE + 1)
             ? serializedIds.substr(TRACE_INFO_SIZE + 1)
             : "";
}

std::optional<std::pair<std::string_view, std::string_view>> ExtractStringIds(
    std::string_view serializedIds) {
  if (serializedIds.size() <= TRACE_INFO_SIZE) {
    return std::nullopt;
  }
  return std::pair(serializedIds.substr(TRACE_ID_OFFSET, TRACE_ID_SIZE),
                   serializedIds.substr(SPAN_ID_OFFSET, SPAN_ID_SIZE));
}

constexpr uint64_t ALL = std::numeric_limits<uint64_t>::max();

void UpdateMask(uint64_t& mask, std::string_view filter) {
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

void TracingOtlpGRPCInit(std::string_view identity) {
  opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
  std::stringstream ss;
  ss << TRACE_ZILLIQA_PORT;

  std::string addr{std::string(TRACE_ZILLIQA_HOSTNAME) + ":" + ss.str()};

  if (!addr.empty()) {
    opts.endpoint = addr;
  }

  resource::ResourceAttributes attributes = {{"service.name", identity}};

  auto resource = resource::Resource::Create(attributes, METRIC_ZILLIQA_SCHEMA);

  auto exporter = otlp::OtlpGrpcExporterFactory::Create(opts);
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);

  trace_api::Provider::SetTracerProvider(provider);
}

void TracingOtlpHTTPInit(std::string_view identity) {
  opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
  std::stringstream ss;
  ss << TRACE_ZILLIQA_PORT;

  std::string addr{std::string(TRACE_ZILLIQA_HOSTNAME) + ":" + ss.str()};

  if (!addr.empty()) {
    opts.url = "http://" + addr + "/v1/traces";
  }

  resource::ResourceAttributes attributes = {{"service.name", identity},
                                             {"version", (uint32_t)1}};

  auto resource = resource::Resource::Create(attributes, METRIC_ZILLIQA_SCHEMA);
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
          std::move(processors), resource);

  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(context);

  trace_api::Provider::SetTracerProvider(provider);

  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

void TracingStdOutInit(std::string_view identity) {
  auto exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  auto processor =
      trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  resource::ResourceAttributes attributes = {{"service.name", identity},
                                             {"version", (uint32_t)1}};
  auto resource = resource::Resource::Create(attributes);
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);

  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(provider);

  // Setup a prpogator
  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          otel_std::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

}  // namespace

bool TracingImpl::Initialize(std::string_view identity,
                             std::string_view filters_mask) {
  std::string_view mask =
      filters_mask.empty() ? TRACE_ZILLIQA_MASK : filters_mask;

  if (mask.empty() || mask == "NONE") {
    // Tracing disabled
    return false;
  }

  uint64_t filtersMask = 0;

  std::vector<std::string_view> flags;
  boost::split(flags, mask, boost::is_any_of(","));
  for (const auto& f : flags) {
    UpdateMask(filtersMask, f);
    if (filtersMask == ALL) {
      break;
    }
  }

  if (filtersMask == 0) {
    // Tracing disabled, corrupted string passed
    LOG_GENERAL(WARNING,
                "Tracing disabled, incorrect filter parameter: " << mask);
    return false;
  }

  try {
    std::string cmp{TRACE_ZILLIQA_PROVIDER};

    transform(cmp.begin(), cmp.end(), cmp.begin(), ::tolower);

    if (cmp == "otlphttp") {
      TracingOtlpHTTPInit(identity);
    } else if (cmp == "otlpgrpc") {
      TracingOtlpGRPCInit(identity);
    } else if (cmp == "stdout") {
      TracingStdOutInit(identity);
    } else {
      LOG_GENERAL(WARNING,
                  "Telemetry provider has defaulted to NOOP provider due to no "
                  "configuration");
      return false;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(
        WARNING,
        "Tracing disabled due to exception while initializing: " << e.what());
    return false;
  } catch (...) {
    LOG_GENERAL(WARNING,
                "Tracing disabled due to unknown exception while initializing");
    return false;
  }

  auto provider = trace_api::Provider::GetTracerProvider();
  assert(provider);
  m_tracer = provider->GetTracer("zilliqa-cpp", OPENTELEMETRY_SDK_VERSION);
  assert(m_tracer);

  m_filtersMask = filtersMask;
  m_identity = identity;
  return true;
}

}  // namespace zil::trace
