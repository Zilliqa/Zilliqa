
add_library(ProtoClient SHARED ${PROTO_HEADER} ${PROTO_SRC} ${GRPC_SRCS} ${GPC_HDRS} Client.cpp)
target_compile_options(ProtoClient PRIVATE "-Wno-unused-parameter")
target_include_directories(ProtoClient
                           SYSTEM PUBLIC
                            ${PROJECT_SOURCE_DIR}/src
                            ${CMAKE_BINARY_DIR}/src/libProtoServer
                            ${CMAKE_BINARY_DIR}/src/libMessage
                            ${CMAKE_BINARY_DIR}/include)

target_link_libraries(ProtoClient
                        PUBLIC
                            ${PROTOBUF_LIBRARY} ${GRPC_LIBRARIES}
                        PRIVATE
                            ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/boringssl/ssl/libssl.a
                            ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/boringssl/crypto/libcrypto.a
                            z ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/cares/cares/lib/libcares.a
                            ${CMAKE_BINARY_DIR}/src/depends/grpc/libaddress_sorting.a)

add_dependencies(ProtoClient ProtoServer)
