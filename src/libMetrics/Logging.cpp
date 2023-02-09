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
#include "Metrics.h"

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"

#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"

namespace otlp_exporter = opentelemetry::exporter::otlp;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace logs = opentelemetry::logs;

namespace {
const double LOGGING_VERSION{8.6};
const std::string ZILLIQA_LOGGING_FAMILY{"zilliqa-cpp"};

}  // namespace

namespace zil {
namespace metrics {
Logging::Logging() {
#if 0
  std::string addr{std::string(LOGGING_ZILLIQA_HOSTNAME) + ":" +
                   std::to_string(LOGGING_ZILLIQA_PORT)};
#endif
  std::string addr = "127.0.0.1:41413";

  otlp_exporter::OtlpHttpLogRecordExporterOptions options;
  if (!addr.empty()) {
    options.url = "http://" + addr + "/v1/logging";
    options.console_debug = true;
    options.content_type = otlp_exporter::HttpRequestContentType::kJson;
  }

  // Create OTLP exporter instance
  auto exporter =
      otlp_exporter::OtlpHttpLogRecordExporterFactory::Create(options);
  auto processor =
      logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<logs::LoggerProvider> provider =
      logs_sdk::LoggerProviderFactory::Create(std::move(processor));

  opentelemetry::logs::Provider::SetLoggerProvider(provider);
}
}  // namespace metrics
}  // namespace zil
