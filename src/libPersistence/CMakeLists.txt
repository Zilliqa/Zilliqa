set(PROTOBUF_IMPORT_DIRS ${PROTOBUF_IMPORT_DIRS} ${PROJECT_SOURCE_DIR}/src/libMessage)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ScillaMessage.proto)

add_library (Persistence ${PROTO_HEADER} ${PROTO_SRC} BlockStorage.cpp Retriever.cpp ContractStorage.cpp)
target_compile_options(Persistence PRIVATE "-Wno-unused-variable")
target_compile_options(Persistence PRIVATE "-Wno-unused-parameter")
target_include_directories (Persistence PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/libPersistence)
target_link_libraries (Persistence PUBLIC Blockchain Trie Constants BlockChainData TraceableDB protobuf::libprotobuf)
