add_library(AccountData
    Account.cpp
    AccountStoreTemp.cpp
    AccountStoreBase.cpp
    AccountStoreSC.cpp
    AccountStoreTrie.cpp
    AccountStore.cpp
    AccountStoreAtomic.cpp
    Transaction.cpp
    LogEntry.cpp
    TransactionReceipt.cpp
    ScillaClient.cpp
    EvmClient.cpp
    InvokeType.h
    AccountStoreSCEvm.cpp)
target_include_directories(AccountData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(AccountData PUBLIC Utils Scilla Block BlockHeader Message Trie TraceableDB EthCrypto PRIVATE EthUtils)
