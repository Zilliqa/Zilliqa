set(PROTOBUF_IMPORT_DIRS ${PROTOBUF_IMPORT_DIRS} ${PROJECT_SOURCE_DIR}/src/libMessage)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ServerRequest.proto ServerResponse.proto ServerMessages.proto Server.proto)
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS Server.proto)
add_library(ProtoServer ${PROTO_HEADER} ${PROTO_SRC} ${GRPC_SRCS} ${GRPC_HDRS} Server.cpp Server.h)
target_compile_options(ProtoServer PRIVATE "-Wno-unused-parameter")

target_include_directories(ProtoServer
                           SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/libProtoServer ${CMAKE_BINARY_DIR}/src/libMessage ${CMAKE_BINARY_DIR}/include)

target_link_libraries(ProtoServer PUBLIC ${PROTOBUF_LIBRARY} ${GRPC_LIBRARIES} AccountData Message
                      ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/boringssl/ssl/libssl.a
                      ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/boringssl/crypto/libcrypto.a
                      z cares
                      ${CMAKE_BINARY_DIR}/src/depends/grpc/libaddress_sorting.a)
