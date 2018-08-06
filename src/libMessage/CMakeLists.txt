protobuf_generate_cpp(PROTO_SRC PROTO_HEADER Message.proto)
add_library (Message SHARED ${PROTO_HEADER} ${PROTO_SRC})
target_include_directories (Message PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Message PUBLIC ${PROTOBUF_LIBRARY})