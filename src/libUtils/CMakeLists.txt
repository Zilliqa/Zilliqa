add_library(Utils BitVector.cpp DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp ShardSizeCalculator.cpp TimeUtils.cpp RootComputation.cpp IPConverter.cpp UpgradeManager.cpp SWInfo.cpp)
target_include_directories(Utils PUBLIC ${PROJECT_SOURCE_DIR}/src Crypto Boost ${G3LOG_INCLUDE_DIRS})
target_link_libraries(Utils INTERFACE Threads::Threads CURL)
target_link_libraries(Utils PUBLIC g3logger Constants MessageSWInfo)
