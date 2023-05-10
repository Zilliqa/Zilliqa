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

#include "Logging.h"
#include "Common.h"
#include "Tracing.h"

#include "libUtils/Logger.h"
#include "libUtils/SWInfo.h"

#include "opentelemetry/exporters/ostream/log_record_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"

#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace otlp_exporter = opentelemetry::exporter::otlp;
namespace logs_exporter = opentelemetry::exporter::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace logs = opentelemetry::logs;

namespace {
const std::string ZILLIQA_LOGGING_FAMILY{"zilliqa-cpp"};

struct OTelLoggingSink {
  static void Shutdown() { m_shutdown = true; }

  void forwardToOTel(g3::LogMessageMover logEntry) {
    // Since both the logger & OTel are essentially singletons and could be
    // destroyed after main() exists, we use this atomic boolean to prevent
    // a crash.
    if (m_shutdown) return;

    auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
    if (!provider) return;

    auto logger =
        provider->GetLogger(ZILLIQA_LOGGING_FAMILY, "zilliqa", VERSION_TAG);
    if (!logger) return;

    auto logRecord = logger->CreateLogRecord();
    if (!logRecord) return;

    const auto& logMessage = logEntry.get();
    if (logMessage._level.value <= G3LOG_DEBUG.value)
      logRecord->SetSeverity(logs::Severity::kDebug);
    else if (logMessage._level.value <= INFO.value)
      logRecord->SetSeverity(logs::Severity::kInfo);
    else if (logMessage._level.value <= WARNING.value)
      logRecord->SetSeverity(logs::Severity::kWarn);
    else if (logMessage._level.value <= FATAL.value)
      logRecord->SetSeverity(logs::Severity::kFatal);

    logRecord->SetBody(
        opentelemetry::common::AttributeValue{logMessage.message()});

    logRecord->SetTimestamp(
        // g3::high_resolution_time_point ->
        // std::chrono::system_clock::time_point
        std::chrono::system_clock::time_point{
            std::chrono::system_clock::duration{
                std::chrono::time_point_cast<
                    std::chrono::system_clock::duration>(logMessage._timestamp)
                    .time_since_epoch()}});

    if (logMessage._extra_data) {
      assert(std::dynamic_pointer_cast<zil::trace::TracingExtraData>(
          logMessage._extra_data));
      const auto& extraData =
          std::static_pointer_cast<zil::trace::TracingExtraData>(
              logMessage._extra_data);
      const auto& tracingIds = extraData->GetTracingIds();
      if (tracingIds) {
        logRecord->SetTraceId(tracingIds->first);
        logRecord->SetSpanId(tracingIds->second);
      }
    }

    logger->EmitLogRecord(std::move(logRecord));
  }

 private:
  static std::atomic_bool m_shutdown;
};

std::atomic_bool OTelLoggingSink::m_shutdown{false};

std::shared_ptr<logs::LoggerProvider> InitFromExporter(
    std::unique_ptr<logs_sdk::LogRecordExporter> exporter) {
  return logs_sdk::LoggerProviderFactory::Create(
      logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter)));
}

std::shared_ptr<logs::LoggerProvider> InitOTHTTP() {
  std::string addr{std::string(LOGGING_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(LOGGING_ZILLIQA_PORT)};

  otlp_exporter::OtlpHttpLogRecordExporterOptions options;
  options.url = "http://" + addr + "/v1/logging";
  options.console_debug = true;
  options.content_type = otlp_exporter::HttpRequestContentType::kJson;

  return InitFromExporter(
      otlp_exporter::OtlpHttpLogRecordExporterFactory::Create(options));
}

std::shared_ptr<logs::LoggerProvider> InitOtlpGrpc() {
  std::string addr{std::string(LOGGING_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(LOGGING_ZILLIQA_PORT)};

  otlp_exporter::OtlpGrpcExporterOptions options;
  options.endpoint = addr;

  return InitFromExporter(
      otlp_exporter::OtlpGrpcLogRecordExporterFactory::Create(options));
}

std::shared_ptr<logs::LoggerProvider> InitStdOut() {
  return InitFromExporter(
      std::make_unique<logs_exporter::OStreamLogRecordExporter>());
}

std::shared_ptr<logs::LoggerProvider> InitNoop() {
  return std::make_shared<opentelemetry::logs::NoopLoggerProvider>();
}

}  // namespace

void Logging::Initialize(std::string_view identity /*= {}*/,
                         std::string provider /*= LOGGING_ZILLIQA_PROVIDER*/) {
  if (!IsObservabilityAllowed(identity)) {
    provider = "NONE";
  }

  boost::algorithm::to_lower(provider);

  std::shared_ptr<logs::LoggerProvider> loggingProvider;

  if (provider == "otlphttp") {
    loggingProvider = InitOTHTTP();
  } else if (provider == "otlpgrpc") {
    loggingProvider = InitOtlpGrpc();
  } else if (provider == "stdout") {
    loggingProvider = InitStdOut();
  } else {
    LOG_GENERAL(
        WARNING,
        "Logging provider has defaulted to NOOP loggingProvider due to no "
        "configuration");
    loggingProvider = InitNoop();
  }

  opentelemetry::logs::Provider::SetLoggerProvider(loggingProvider);

  Logger::GetLogger().AddSink(std::make_unique<OTelLoggingSink>(),
                              &OTelLoggingSink::forwardToOTel);
}

void Logging::Shutdown() {
  OTelLoggingSink::Shutdown();
  opentelemetry::logs::Provider::SetLoggerProvider(
      std::make_unique<opentelemetry::logs::NoopLoggerProvider>());
}
