add_library(Block DSBlock.cpp MicroBlock.cpp TxBlock.cpp VCBlock.cpp)
target_include_directories(Block PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Block PUBLIC Trie ${Schnorr_LIBRARY})
