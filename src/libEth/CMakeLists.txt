add_library(Eth Eth.cpp)
target_include_directories(Eth PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Eth EthCrypto AccountData)

add_library(Filters
        filters/FiltersImpl.cpp
        filters/FiltersUtils.cpp
        filters/PendingTxnCache.cpp
        filters/BlocksCache.cpp
        filters/APICache.cpp
        )

target_link_libraries(Filters Utils jsonrpc::client)
