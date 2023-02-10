protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ZilliqaMessage.proto)

add_library (MessageCommon STATIC ${PROTO_SRC} MessengerCommon.cpp)
target_include_directories (MessageCommon PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries (MessageCommon PUBLIC protobuf::libprotobuf)

add_library (Message STATIC Messenger.cpp MessengerAccountStoreBase.cpp MessengerAccountStoreTrie.cpp)
target_include_directories (Message PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries (Message PUBLIC MessageCommon AccountStore AccountData Blockchain MiningData CoinbaseData Network)

add_library (MessageSWInfo STATIC MessengerSWInfo.cpp)
target_include_directories (MessageSWInfo PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries (MessageSWInfo PUBLIC MessageCommon)
