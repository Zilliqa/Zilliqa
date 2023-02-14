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

#ifndef ZILLIQA_SRC_LIBMETRICS_HELPER_H_
#define ZILLIQA_SRC_LIBMETRICS_HELPER_H_

#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_context.h>
#include <string>

namespace zil::trace {

opentelemetry::trace::SpanContext ExtractSpanContextFromTraceInfo(
    const std::string& traceInfo);

/// Extract info to continue spans with distributed tracing
void ExtractTraceInfoFromActiveSpan(std::string& serializedTraceInfo);

/// Creates child span from serialized trace info
std::shared_ptr<opentelemetry::trace::Span> CreateChildSpan(
    std::string_view name, const std::string& serializedTraceInfo);

}  // namespace zil::trace

#endif  // ZILLIQA_SRC_LIBMETRICS_HELPER_H_
