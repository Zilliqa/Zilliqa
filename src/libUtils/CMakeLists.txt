find_package(CURL REQUIRED)

add_library(Utils AddressConversion.cpp BitVector.cpp DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp ShardSizeCalculator.cpp TimeUtils.cpp RandomGenerator.cpp RootComputation.cpp IPConverter.cpp UpgradeManager.cpp SWInfo.cpp FileSystem.cpp ScillaUtils.cpp MemoryStats.cpp CommonUtils.cpp EvmUtils.cpp EvmUtils.h EvmJsonResponse.h EvmJsonResponse.cpp EvmJsonResponse.h)
target_include_directories(Utils
  INTERFACE ${CURL_INCLUDE_DIRS}
  PUBLIC ${PROJECT_SOURCE_DIR}/src Boost)
target_link_libraries(Utils INTERFACE Threads::Threads CURL::libcurl ${PYTHON_LIBRARIES})
target_link_libraries(Utils PUBLIC g3logger CryptoUtils Constants MessageSWInfo websocketpp::websocketpp jsonrpc::common jsoncpp_lib)
