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

#ifndef ZILLIQA_SRC_LIBUTILS_TRACING_H_
#define ZILLIQA_SRC_LIBUTILS_TRACING_H_

#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include <cassert>

#include "common/Singleton.h"
#include "common/TraceFilters.h"

namespace trace_api      = opentelemetry::trace;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;

namespace zil {
namespace trace {
class Filter : public Singleton<Filter> {
 public:

  Filter(){ init();}

  void init();

  bool Enabled(FilterClass to_test) {
    return m_mask & (1 << static_cast<int>(to_test));
  }

 private:
  uint64_t m_mask{};
};

}
}

class Tracing : public Singleton<Tracing> {
 public:
  Tracing();

  std::string Version() { return "Initial" ;}

  std::shared_ptr<trace_api::Tracer> get_tracer();

  /// Called on main() exit explicitly
  void Shutdown();

 private:
  void Init();
  void ZipkinInit();
  void StdOutInit();
  void ZPagesInit();
  void JaegerInit();
  void OtlpGRPCInit();
  void OtlpHTTPInit();

  std::shared_ptr<opentelemetry::trace::TracerProvider> m_provider;
};

#define TRACE_ENABLED(FILTER_CLASS)          \
  zil::trace::Filter::GetInstance().Enabled( \
      zil::trace::FilterClass::FILTER_CLASS)

#define SCOPED_SPAN(FILTER_CLASS, SCOPE_NAME, SPAN)          \
  trace_api::Scope SCOPE_NAME = TRACE_ENABLED(FILTER_CLASS)  \
                                    ? trace_api::Scope(SPAN) \
                                    : trace_api::Scope(nullptr);

#define START_SPAN(FILTER_CLASS, ATTRIBUTES)                                 \
  TRACE_ENABLED(FILTER_CLASS)                                                \
  ? Tracing::GetInstance().get_tracer()->StartSpan(__FUNCTION__, ATTRIBUTES) \
  : trace_api::Tracer::GetCurrentSpan()

#define TRACE_EVENT(SPAN,FILTER_CLASS,CLASS,ATTRIBUTES) \
  TRACE_ENABLED(FILTER_CLASS)                    \
  ? SPAN->AddEvent(CLASS,ATTRIBUTES) : SPAN->AddEvent(CLASS,{})

using TRACE_ATTRIBUTE=std::map<std::string, opentelemetry::common::AttributeValue>;


#define START_SPAN_WITH_PARENT(FILTER_CLASS, ATTRIBUTES, OPTIONS)            \
  TRACE_ENABLED(FILTER_CLASS)                                                \
  ? Tracing::GetInstance().get_tracer()->StartSpan(__FUNCTION__, ATTRIBUTES, \
                                                   OPTIONS)                  \
  : trace_api::Tracer::GetCurrentSpan()

#endif  // ZILLIQA_SRC_LIBUTILS_TRACING_H_
