add_library (Zilliqa Zilliqa.cpp)
target_include_directories (Zilliqa PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/libProtoServer ${CMAKE_BINARY_DIR}/src/libMessage)
target_link_libraries (Zilliqa PUBLIC Consensus Crypto DB Lookup Mediator Network Node Server ProtoServer)
