add_library(BlockChainData DSBlockChain.cpp TxBlockChain.cpp VCBlockChain.cpp)
target_include_directories(BlockChainData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (BlockChainData PUBLIC Block BlockHeader Crypto Trie)