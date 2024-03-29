find_package(CURL REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)

add_library(Metrics STATIC
  Logging.cpp
  Metrics.cpp
  Tracing.cpp
  Api.h
  Metrics.h
  Tracing.h
  Common.h
  internal/mixins.h Api.cpp internal/source_location.h)

target_include_directories(Metrics PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src ${CURL_INCLUDE_DIRS})
target_link_libraries(Metrics
        INTERFACE
        Threads::Threads
        CURL::libcurl
        PUBLIC
        Utils
        protobuf::libprotobuf
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::logs
        opentelemetry-cpp::trace
        opentelemetry-cpp::ostream_span_exporter
        opentelemetry-cpp::ostream_log_record_exporter
        opentelemetry-cpp::ostream_metrics_exporter
        opentelemetry-cpp::otlp_http_metric_exporter
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::otlp_http_log_record_exporter
        opentelemetry-cpp::prometheus_exporter
        opentelemetry-cpp::otlp_grpc_metrics_exporter
        opentelemetry-cpp::otlp_grpc_exporter
        opentelemetry-cpp::otlp_grpc_log_record_exporter)
