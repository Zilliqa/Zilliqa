add_library (Persistence BlockStorage.cpp DB.cpp Retriever.cpp ContractStorage.cpp)
target_include_directories (Persistence PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Persistence PUBLIC AccountData Crypto ${LevelDB_LIBRARIES} ${SNAPPY_LIBRARIES} Trie Utils Constants BlockChainData)
