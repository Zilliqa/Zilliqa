add_library(AccountData Account.cpp AccountStoreTemp.cpp AccountStoreBase.tpp AccountStoreSC.tpp AccountStoreTrie.tpp AccountStore.cpp AccountStoreAtomic.tpp Transaction.cpp LogEntry.cpp TransactionReceipt.cpp)
target_include_directories(AccountData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (AccountData PUBLIC Block BlockHeader Crypto Message Trie Utils Persistence jsoncpp)
