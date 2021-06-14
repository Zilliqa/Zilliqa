add_library(AccountData Account.cpp AccountStoreTemp.cpp AccountStoreBase.tpp AccountStoreSC.tpp AccountStoreTrie.tpp AccountStore.cpp AccountStoreAtomic.tpp Transaction.cpp LogEntry.cpp TransactionReceipt.cpp ScillaClient.cpp BloomFilter.cpp)
target_include_directories(AccountData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (AccountData PUBLIC Server Block BlockHeader Message Trie Utils Persistence TraceableDB ${JSONCPP_LINK_TARGETS})
