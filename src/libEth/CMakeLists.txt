add_library(Eth Eth.cpp)
target_include_directories(Eth PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Eth EthCrypto AccountData)
