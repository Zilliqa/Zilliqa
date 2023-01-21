cmake_minimum_required(VERSION 3.22)
project(otel)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

add_compile_options(-Wall)
add_compile_options(-Werror)
add_compile_options(-pedantic)
add_compile_options(-Wextra)

find_package(CURL REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(protobuf REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wno-unused-parameter)

include_directories(. src)

set_target_properties(opentelemetry-cpp::opentelemetry_exporter_prometheus PROPERTIES
        INTERFACE_LINK_LIBRARIES "opentelemetry-cpp::metrics"
        )

add_executable(otel main.cpp src/libUtils/Metrics.cpp src/libUtils/Tracing.cpp src/libUtils/Metrics.h src/libUtils/Tracing.h)

target_link_libraries(otel
        INTERFACE
        Threads::Threads
        CURL::libcurl
        PUBLIC
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::logs
        opentelemetry-cpp::trace
        opentelemetry-cpp::ostream_span_exporter
        opentelemetry-cpp::ostream_metrics_exporter
        opentelemetry-cpp::otlp_http_metric_exporter
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::opentelemetry_exporter_prometheus
        )

message(STATUS "We are on a ${CMAKE_SYSTEM_NAME} system")




