set(CMAKE_CXX_STANDARD 20)

add_library(AccountStore
        AccountStoreTemp.cpp
        AccountStoreBase.cpp
        AccountStoreSC.cpp
        AccountStore.cpp
        AccountStoreAtomic.cpp
        AccountStoreSCEvm.cpp
        services/evm/EvmProcessContext.cpp
        services/evm/EvmClient.cpp
        ../../libData/AccountData/LogEntry.cpp)
target_include_directories(AccountStore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(AccountStore PUBLIC AccountData Utils Scilla Blockchain Message Trie TraceableDB EthCrypto PRIVATE Cps EthUtils)
