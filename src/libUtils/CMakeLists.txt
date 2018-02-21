include_directories(${Boost_INCLUDE_DIRS}) 
add_library (Utils DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp TimeUtils.cpp TxnRootComputation.cpp)
target_include_directories (Utils PUBLIC ${PROJECT_SOURCE_DIR}/src)