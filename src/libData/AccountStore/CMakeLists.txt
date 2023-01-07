add_library(AccountStore
        AccountStoreTemp.cpp
        AccountStoreBase.cpp
        AccountStoreSC.cpp
        AccountStoreTrie.cpp
        AccountStore.cpp
        AccountStoreAtomic.cpp
        AccountStoreSCEvm.cpp
        services/evm/EvmProcessContext.cpp
        services/evm/EvmClient.cpp
        services/scilla/ScillaClient.cpp
        ../../libScilla/UnixDomainSocketServer.cpp
        )
target_include_directories(AccountStore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(AccountStore PUBLIC AccountData Utils Scilla Block BlockHeader Message Trie TraceableDB EthCrypto PRIVATE EthUtils)
