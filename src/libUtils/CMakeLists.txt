find_package(CURL REQUIRED)
find_package(protobuf CONFIG REQUIRED)


protobuf_generate_cpp(PROTO_SRC PROTO_HEADER Evm.proto)

add_library(Utils
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
        PRIVATE
        ${CryptoUtils_LIBRARY}
        ${g3sinks_LOGROTATE_LIBRARY}
        websocketpp::websocketpp
        Boost::filesystem)
