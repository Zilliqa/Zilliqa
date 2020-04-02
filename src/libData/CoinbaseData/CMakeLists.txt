add_library(CoinbaseData CoinbaseStruct.cpp)
target_include_directories(CoinbaseData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (CoinbaseData PUBLIC Utils)