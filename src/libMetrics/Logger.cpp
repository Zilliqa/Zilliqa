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
#ifdef ENABLE_LOGS_PREVIEW
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace trace = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;
namespace otlp = opentelemetry::exporter::otlp;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace logs = opentelemetry::logs;
namespace resource = opentelemetry::sdk::resource;
namespace logs = opentelemetry::logs;
namespace trace = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;

namespace Metrics {

opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions logger_opts;
void InitLogger() {
  logger_opts.console_debug = true;
  // Create OTLP exporter instance
  auto exporter = otlp::OtlpHttpLogRecordExporterFactory::Create(logger_opts);
  auto processor =
      logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<logs::LoggerProvider> provider =
      logs_sdk::LoggerProviderFactory::Create(std::move(processor));

  opentelemetry::logs::Provider::SetLoggerProvider(provider);
}

nostd::shared_ptr<logs::Logger> get_logger() {
  auto provider = logs::Provider::GetLoggerProvider();
  return provider->GetLogger("otel_logger", "", "hashset");
}

void log(std::string /*msg*/) {
  std::cout << "Logging..." << std::endl;
  /*
    auto span = get_tracer()->StartSpan("log span");
    auto scoped_span = trace::Scope(get_tracer()->StartSpan("hash_set"));
    auto ctx = span->GetContext();
    auto logger = get_logger();
    logger->Log(opentelemetry::logs::Severity::kDebug, msg, {}, ctx.trace_id(),
    ctx.span_id(), ctx.trace_flags(),
                opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
                */
}

}  // namespace Metrics

#else
int main() { return 0; }
#endif
