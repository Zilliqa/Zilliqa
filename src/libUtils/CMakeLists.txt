add_library(Utils BitVector.cpp DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp ShardSizeCalculator.cpp TimeUtils.cpp TxnRootComputation.cpp IPConverter.cpp UpgradeManager.cpp)
target_include_directories(Utils PUBLIC ${PROJECT_SOURCE_DIR}/src Crypto Boost ${G3LOG_INCLUDE_DIRS})
target_link_libraries(Utils INTERFACE Threads::Threads)
target_link_libraries(Utils PUBLIC g3logger)
