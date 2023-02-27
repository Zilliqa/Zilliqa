add_library(Eth Eth.cpp)
target_include_directories(Eth PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Eth PUBLIC AccountData PRIVATE EthCrypto)

add_library(Filters
    filters/FiltersImpl.cpp
    filters/SubscriptionsImpl.cpp
    filters/FiltersUtils.cpp
    filters/PendingTxnCache.cpp
    filters/BlocksCache.cpp
    filters/APICache.cpp
    filters/PendingTxnUpdater.cpp
    )
add_library(EthUtils
    utils/EthUtils.cpp
    )
target_include_directories(EthUtils PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(Filters Utils)
