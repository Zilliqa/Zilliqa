add_library(Block STATIC DSBlock.cpp MicroBlock.cpp TxBlock.cpp VCBlock.cpp Serialization.cpp)
target_include_directories(Block PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Block PUBLIC Trie ${Schnorr_LIBRARY})
