protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ZilliqaMessage.proto ErrorMsg.proto)

add_library (Message ${PROTO_HEADER} ${PROTO_SRC} Messenger.cpp MessengerAccountStoreBase.cpp)
target_compile_options(Message PRIVATE "-Wno-unused-parameter")
target_include_directories (Message PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries (Message PUBLIC ${PROTOBUF_LIBRARY} AccountData Block BlockHeader MiningData Utils)

add_library (MessageSWInfo ${PROTO_HEADER} ${PROTO_SRC} MessengerSWInfo.cpp)
target_compile_options(MessageSWInfo PRIVATE "-Wno-unused-parameter")
target_include_directories (MessageSWInfo PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries (MessageSWInfo PUBLIC ${PROTOBUF_LIBRARY} Utils)

add_library (MessageErrorMsg ${PROTO_HEADER} ${PROTO_SRC} MessengerErrorMsg.h)
target_compile_options(MessageErrorMsg PRIVATE "-Wno-unused-parameter")
target_include_directories (MessageErrorMsg PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_link_libraries (MessageErrorMsg PUBLIC ${PROTOBUF_LIBRARY})
