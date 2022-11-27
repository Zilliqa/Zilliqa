find_package(CURL REQUIRED)

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER Evm.proto)

add_library(Utils AddressConversion.cpp BitVector.cpp DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp ShardSizeCalculator.cpp TimeUtils.cpp RandomGenerator.cpp RootComputation.cpp IPConverter.cpp UpgradeManager.cpp SWInfo.cpp FileSystem.cpp ScillaUtils.cpp MemoryStats.cpp CommonUtils.cpp EvmUtils.cpp EvmUtils.h ${PROTO_SRC})
target_include_directories(Utils PUBLIC ${PROJECT_SOURCE_DIR}/src ${CURL_INCLUDE_DIRS})
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
    jsonrpc::common
    jsoncpp_lib
    protobuf::libprotobuf
  PRIVATE
    ${CryptoUtils_LIBRARY}
    ${g3sinks_LOGROTATE_LIBRARY})

