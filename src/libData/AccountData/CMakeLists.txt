add_library(AccountData
    Account.cpp
    Transaction.cpp
    LogEntry.cpp
    TransactionReceipt.cpp
    InvokeType.h
        ../AccountStore/services/evm/EvmProcessContext.cpp)
target_include_directories(AccountData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(AccountData PUBLIC AccountStore Utils Scilla Block BlockHeader Message Trie TraceableDB EthCrypto PRIVATE EthUtils)
