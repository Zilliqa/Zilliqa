find_package(CURL REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)

# A workaround for incorrect link libraries in vcpkg's opentelemtry-cpp-target.cmake
# which includes non-existent target (i.e. opentelemetry-cpp::pull & opentelemetry-cpp::core).
set_target_properties(opentelemetry-cpp::opentelemetry_exporter_prometheus PROPERTIES
        INTERFACE_LINK_LIBRARIES "opentelemetry-cpp::metrics"
        )

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER Evm.proto)

add_library(Utils
        AddressConversion.cpp
        BitVector.cpp
        DataConversion.cpp
        Logger.cpp
        ShardSizeCalculator.cpp
        TimeUtils.cpp
        RandomGenerator.cpp
        IPConverter.cpp
        UpgradeManager.cpp
        SWInfo.cpp
        FileSystem.cpp
        MemoryStats.cpp
        CommonUtils.cpp
        EvmUtils.cpp
        Metrics.cpp
        Tracing.cpp
        ${PROTO_SRC})

target_include_directories(Utils PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src ${CURL_INCLUDE_DIRS})
target_link_libraries(Utils
        INTERFACE
        Threads::Threads
        CURL::libcurl
        PUBLIC
        g3log
        Constants
        MessageSWInfo
        jsonrpc
        jsoncpp_lib
        protobuf::libprotobuf
        opentelemetry-cpp::zpages
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::logs
        opentelemetry-cpp::trace
        opentelemetry-cpp::ostream_span_exporter
        opentelemetry-cpp::ostream_metrics_exporter
        opentelemetry-cpp::otlp_http_metric_exporter
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::opentelemetry_exporter_prometheus
        opentelemetry-cpp::jaeger_trace_exporter
        opentelemetry-cpp::zpages
        nlohmann_json
        PRIVATE
        ${CryptoUtils_LIBRARY}
        ${g3sinks_LOGROTATE_LIBRARY}
        websocketpp::websocketpp
        Boost::filesystem)
