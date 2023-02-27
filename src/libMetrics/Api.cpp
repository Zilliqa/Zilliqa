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

#include "Api.h"
#include <g3log/loglevels.hpp>
#include "libUtils/Logger.h"

// The OpenTelemetry Metrics Interface.

namespace zil {
namespace observability {
namespace api {

Z_I64METRIC &GetErrorHistogram() {
  static Z_I64METRIC counter{
      zil::metrics::FilterClass::GLOBAL_ERROR, "err",
      "A history of monitically numbered errors that are linked to traces", ""};
  return counter;
}

uint64_t GetNextErrorCount() {
  static uint64_t counter{0};
  return ++counter;
}

void EventMetricTrace(const std::string msg, std::string funcName, int line, int errno) {
  if (zil::metrics::Filter::GetInstance().Enabled(
          zil::metrics::FilterClass::GLOBAL_ERROR)) {
    if (zil::trace::Tracing::IsEnabled()) {
      zil::trace::Span activeSpan = zil::trace::Tracing::GetActiveSpan();
      auto spanIds = zil::trace::Tracing::GetActiveSpanStringIds();
      if (spanIds) {
        activeSpan.AddEvent("Error", {{"error", msg}});
        GetErrorHistogram().IncrementAttr(
            {{"trace_id", spanIds->first}, {"span_id", spanIds->second}});
        activeSpan.AddEvent("Error", {{"error", msg}});
      }
    }
  }
  LOG_GENERAL(INFO, msg);
}

void EventTrace(const std::string& eventname, const std::string& topic, const std::string& value) {
  if (zil::trace::Tracing::IsEnabled() && zil::trace::Tracing::HasActiveSpan()) {
      zil::trace::Tracing::GetActiveSpan().AddEvent(eventname, {{topic, value}});
  }
}


}  // namespace api

}  // namespace observability
}  // namespace zil
