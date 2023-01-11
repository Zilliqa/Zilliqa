add_library(BlockHeader BlockHeaderBase.cpp DSBlockHeader.cpp MicroBlockHeader.cpp TxBlockHeader.cpp VCBlockHeader.cpp Serialization.cpp)
target_include_directories(Block PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (BlockHeader PUBLIC ${Schnorr_LIBRARY} PRIVATE MessageCommon)
