add_library(Utils BitVector.cpp DataConversion.cpp Logger.cpp SanityChecks.cpp Scheduler.cpp TimeUtils.cpp TxnRootComputationLight.cpp)
target_include_directories(Utils PUBLIC ${PROJECT_SOURCE_DIR}/src Crypto Boost)
target_link_libraries(Utils INTERFACE Threads::Threads)
