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

#ifndef ZILLIQA_SRC_LIBMETRICS_TRACING2_H_
#define ZILLIQA_SRC_LIBMETRICS_TRACING2_H_

#include <memory>
#include <span>
#include <string>
#include <variant>

#include <opentelemetry/trace/span_id.h>
#include <opentelemetry/trace/trace_id.h>

// Currently maxes out at 64 filters, in order to increase developer should
// change the type of the mask from uint64_t to uint128_t or uint256_t if
// the number of filters ever increases beyond 64.
//
// Do not override the default numbering of these items, the algorithms rely
// upon these definitions being consecutive, so no assigning new numbers.

// To extend filter classes, you may add items, the total number is limited to
// 64 (bit mask)
#define TRACE_FILTER_CLASSES(T) \
  T(EVM_CLIENT)                 \
  T(EVM_CLIENT_LOW_LEVEL)       \
  T(SCILLA_PROCESSING)          \
  T(SCILLA_IPC)                 \
  T(EVM_RPC)                    \
  T(LOOKUP_SERVER)              \
  T(QUEUE)                      \
  T(ACC_EVM)                    \
  T(NODE)                       \
  T(ACC_HISTOGRAM)

// TODO XXX '2' to be removed after api stabilizes
namespace zil::trace2 {

enum class FilterClass {
#define ENUM_FILTER_CLASS(C) C,
  TRACE_FILTER_CLASSES(ENUM_FILTER_CLASS)
#undef ENUM_FILTER_CLASS
      FILTER_CLASS_END
};

using Value =
    std::variant<bool, int64_t, uint64_t, double, const char*, std::string_view,
                 std::span<const bool>, std::span<const int64_t>,
                 std::span<const uint64_t>, std::span<const double>,
                 std::span<const std::string_view>>;

using opentelemetry::trace::SpanId;
using opentelemetry::trace::TraceId;

// StatusCode - Represents the canonical set of status codes of a finished Span.
enum class StatusCode {
  UNSET,  // default status
  OK,     // Operation has completed successfully.
  ERROR   // The operation contains an error
};

class Span {
  class Impl {
   public:
    virtual ~Impl() noexcept = default;

    virtual bool IsRecording() const noexcept = 0;

    virtual SpanId GetSpanId() const noexcept = 0;

    virtual TraceId GetTraceId() const noexcept = 0;

    virtual const std::string& GetIds() const noexcept = 0;

    virtual void SetAttribute(std::string_view name, Value value) noexcept = 0;

    virtual void AddEvent(
        std::string_view name,
        std::initializer_list<std::pair<std::string_view, Value>>
            attributes) noexcept = 0;

    virtual void End(StatusCode status = StatusCode::UNSET) noexcept = 0;
  };

  // Null for disabled spans and no-op
  std::shared_ptr<Impl> m_impl;

  // If true, the span will be deactivated in dtor
  bool m_isScoped = false;

  // Can be constructed from TracingImpl only
  friend class TracingImpl;
  friend class Tracing;

  Span(std::shared_ptr<Impl> impl, bool scoped)
      : m_impl(std::move(impl)), m_isScoped(scoped) {}

 public:
  // Creates a no-op span
  Span() = default;

  Span(const Span&) = delete;
  Span& operator=(const Span&) = delete;
  Span(Span&&) = delete;
  Span& operator=(Span&&) = delete;

  ~Span() { End(); }

  bool IsRecording() const { return m_impl && m_impl->IsRecording(); }

  /// Returns serialized IDs of the span if it's valid, empty string otherwise.
  /// The string can be utilized as remote_trace_info in other threads or
  /// processes or remote nodes in Tracing::CreateChildSpanOfRemoteTrace(...)
  const std::string& GetIds() const {
    static const std::string empty;
    return m_impl ? m_impl->GetIds() : empty;
  }

  SpanId GetSpanId() const { return m_impl ? m_impl->GetSpanId() : SpanId(); }

  TraceId GetTraceId() const {
    return m_impl ? m_impl->GetTraceId() : TraceId();
  }

  void SetAttribute(std::string_view name, Value value) {
    if (m_impl) {
      m_impl->SetAttribute(name, value);
    }
  }

  void AddEvent(
      std::string_view name,
      std::initializer_list<std::pair<std::string_view, Value>> attributes) {
    if (m_impl) {
      m_impl->AddEvent(name, attributes);
    }
  }

  void End(StatusCode status = StatusCode::UNSET) {
    if (m_isScoped && m_impl) {
      m_impl->End(status);
      m_impl.reset();
    }
  }
};

class Tracing {
 public:
  /// Initializes the tracing engine only if it's not initialized at the moment.
  /// Can be (optionally) called before the first usage to see logs and
  /// initialization result
  /// \param filters_mask If empty then config value is used
  /// \return Success of initialization. If 'false' is returned, then
  /// the tracing will be disabled
  static bool Initialize(std::string_view global_name = {},
                         std::string_view filters_mask = {});

  /// Returns if tracing with a given filter is enabled. Usable for more complex
  /// scenarios than just CreateSpan(...)
  static bool IsEnabled(FilterClass filter);

  /// Creates a scoped span.
  /// Returns a no-op span if this filter is disabled or tracing is disabled.
  /// Otherwise creates a child span of the active span (if there is the active
  /// span in this thread) and activates it
  static Span CreateSpan(FilterClass filter, std::string_view name);

  /// Creates a scoped span as a child of remote span.
  /// Returns a no-op span if deserialization of remote_trace_info fails.
  /// All the rest logic is the same as of CreateSpan
  static Span CreateChildSpanOfRemoteTrace(FilterClass filter,
                                           std::string_view name,
                                           std::string_view remote_trace_info);

  /// Returns the active span (if any) or to a no-op span (if no
  /// active span or tracing disabled)
  static Span GetActiveSpan();

  // TODO some research needed to shutdown it gracefully
  //static void Shutdown();
};

}  // namespace zil::trace2

#endif  // ZILLIQA_SRC_LIBMETRICS_TRACING2_H_
