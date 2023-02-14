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

#include <span>
#include "Api.h"

#include "common/Constants.h"
#include "libUtils/Logger.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/trace/propagation/b3_propagator.h"
#include "opentelemetry/trace/provider.h"


namespace {
constexpr size_t FLAGS_OFFSET = 0;
constexpr size_t FLAGS_SIZE = 2;
constexpr size_t SPAN_ID_OFFSET = FLAGS_SIZE + 1;
constexpr size_t SPAN_ID_SIZE = 16;
constexpr size_t TRACE_ID_OFFSET = SPAN_ID_OFFSET + SPAN_ID_SIZE + 1;
constexpr size_t TRACE_ID_SIZE = 32;
constexpr size_t TRACE_INFO_SIZE = FLAGS_SIZE + 1 + SPAN_ID_SIZE + 1 + TRACE_ID_SIZE;
}  // namespace

std::string ExtractTraceInfoFromActiveSpan() {
  auto activeSpan = Tracing::GetInstance().get_tracer()->GetCurrentSpan();
  auto spanContext = activeSpan->GetContext();
  if (!spanContext.IsValid()) {
    LOG_GENERAL(WARNING, "No active spans");
    return {};
  }

  std::string result(TRACE_INFO_SIZE, '-');

  spanContext.trace_flags().ToLowerBase16(std::span<char, FLAGS_SIZE>(result.data() + FLAGS_OFFSET, FLAGS_SIZE));
  spanContext.span_id().ToLowerBase16(std::span<char, SPAN_ID_SIZE>(result.data() + SPAN_ID_OFFSET, SPAN_ID_SIZE));
  spanContext.trace_id().ToLowerBase16(std::span<char, TRACE_ID_SIZE>(result.data() + TRACE_ID_OFFSET, TRACE_ID_SIZE));

  return result;
}

trace_api::SpanContext ExtractSpanContextFromTraceInfo(const std::string &traceInfo) {
  if (traceInfo.size() != TRACE_INFO_SIZE) {
    LOG_GENERAL(WARNING, "Unexpected trace info size " << traceInfo.size());
    return trace_api::SpanContext::GetInvalid();
  }

  if (traceInfo[SPAN_ID_OFFSET - 1] != '-' || traceInfo[TRACE_ID_OFFSET - 1] != '-') {
    LOG_GENERAL(WARNING, "Invalid format of trace info " << traceInfo);
    return trace_api::SpanContext::GetInvalid();
  }

  std::string_view trace_id_hex(traceInfo.data() + TRACE_ID_OFFSET, TRACE_ID_SIZE);
  std::string_view span_id_hex(traceInfo.data() + SPAN_ID_OFFSET, SPAN_ID_SIZE);
  std::string_view trace_flags_hex(traceInfo.data() + FLAGS_OFFSET, FLAGS_SIZE);

  using trace_api::propagation::detail::IsValidHex;

  if (!IsValidHex(trace_id_hex) || !IsValidHex(span_id_hex) || !IsValidHex(trace_flags_hex)) {
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

std::shared_ptr<trace_api::Span> CreateChildSpan(std::string_view name, const std::string &traceInfo) {
  auto spanCtx = ExtractSpanContextFromTraceInfo(traceInfo);
  if (!spanCtx.IsValid()) {
    return trace_api::Tracer::GetCurrentSpan();
  }
  //  std::shared_ptr<trace_api::Span> parent = std::make_shared<trace_api::DefaultSpan>(spanCtx);

  trace_api::StartSpanOptions options;

  // child spans from deserialized parent  are of server kind
  options.kind = trace_api::SpanKind::kServer;
  options.parent = spanCtx;  // trace_api::GetSpan(new_context)->GetContext();

  return Tracing::GetInstance().get_tracer()->StartSpan(name, options);
}