add_library(BlockChainData BlockLinkChain.cpp)
target_include_directories(BlockChainData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (BlockChainData PUBLIC Message)