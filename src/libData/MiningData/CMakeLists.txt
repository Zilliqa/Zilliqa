add_library(MiningData DSPowSolution.cpp)
target_include_directories(MiningData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (MiningData PUBLIC Message Utils)