add_library(Block BlockBase.cpp DSBlock.cpp MicroBlock.cpp TxBlock.cpp VCBlock.cpp)
target_include_directories(Block PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Block PUBLIC Consensus Trie)