add_library (Zilliqa Zilliqa.cpp)
target_include_directories (Zilliqa PUBLIC ${PROJECT_SOURCE_DIR}/src ${JSONRPC_INCLUDE_DIR})
add_dependencies(Zilliqa jsonrpc-project)
target_link_libraries (Zilliqa PUBLIC Consensus Crypto Lookup Mediator Network Node Server)
