add_library(Lookup Lookup.cpp Synchronizer.cpp)
add_dependencies(Lookup jsonrpc-project)
target_include_directories(Lookup PUBLIC ${PROJECT_SOURCE_DIR}/src ${JSONRPC_INCLUDE_DIR})
target_link_libraries (Lookup PUBLIC AccountData Network Constants BlockChainData POW)
