add_library(BlockHeader BlockHeaderBase.cpp DSBlockHeader.cpp MicroBlockHeader.cpp TxBlockHeader.cpp VCBlockHeader.cpp FallbackBlockHeader.cpp)
target_include_directories(Block PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (BlockHeader PUBLIC Crypto Trie)