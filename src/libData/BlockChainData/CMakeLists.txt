add_library(BlockChainData DSBlockChain.cpp TxBlockChain.cpp)
target_include_directories(BlockChainData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (BlockChainData LINK_PUBLIC Block BlockHeader Crypto Trie)