find_package(CURL REQUIRED)
find_package(opentelemetry-cpp REQUIRED)

# A workaround for incorrect link libraries in vcpkg's opentelemtry-cpp-target.cmake
# which includes non-existent target (i.e. opentelemetry-cpp::pull & opentelemetry-cpp::core).
set_target_properties(opentelemetry-cpp::opentelemetry_exporter_prometheus PROPERTIES
  INTERFACE_LINK_LIBRARIES "opentelemetry-cpp::metrics"
)

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER Evm.proto)

add_library(Utils AddressConversion.cpp BitVector.cpp DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp ShardSizeCalculator.cpp TimeUtils.cpp RandomGenerator.cpp RootComputation.cpp IPConverter.cpp UpgradeManager.cpp SWInfo.cpp FileSystem.cpp ScillaUtils.cpp MemoryStats.cpp CommonUtils.cpp EvmUtils.cpp Metrics.cpp ${PROTO_SRC})
target_include_directories(Utils PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src ${CURL_INCLUDE_DIRS})
target_link_libraries(Utils
  INTERFACE
    Threads::Threads
    CURL::libcurl
    Python3::Python
  PUBLIC
    g3log
    Constants
    MessageSWInfo
    websocketpp::websocketpp
    jsonrpc
    jsoncpp_lib
    protobuf::libprotobuf
    opentelemetry-cpp::api
    opentelemetry-cpp::sdk
    opentelemetry-cpp::logs
    opentelemetry-cpp::opentelemetry_exporter_prometheus
  PRIVATE
    ${CryptoUtils_LIBRARY}
    ${g3sinks_LOGROTATE_LIBRARY}
    Boost::filesystem)
