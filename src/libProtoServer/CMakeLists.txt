set(PROTOBUF_IMPORT_DIRS ${PROTOBUF_IMPORT_DIRS} ${PROJECT_SOURCE_DIR}/src/libMessage)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ServerRequest.proto ServerResponse.proto ServerMessages.proto Server.proto)
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS Server.proto)

add_library(ProtoServer SHARED ${PROTO_HEADER} ${PROTO_SRC} ${GRPC_SRCS} ${GPC_HDRS} Server.cpp)
target_compile_options(ProtoServer PRIVATE "-Wno-unused-parameter")
target_include_directories(ProtoServer
                            SYSTEM PUBLIC
                                ${PROJECT_SOURCE_DIR}/src
                                ${CMAKE_BINARY_DIR}/src/libProtoServer
                                ${CMAKE_BINARY_DIR}/src/libMessage
                                ${CMAKE_BINARY_DIR}/include)


if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(ProtoServer
                            PUBLIC
                                ${PROTOBUF_LIBRARY} ${GRPC_LIBRARIES}
                            PRIVATE
                                boringssl_ssl boringssl_crypto grpc_z cares address_sorting)

    add_dependencies(ProtoServer AccountData Message Lookup POW DirectoryService Node Mediator)

else()
    target_link_libraries(ProtoServer
                            PUBLIC
                                ${PROTOBUF_LIBRARY} ${GRPC_LIBRARIES} AccountData Message Lookup POW DirectoryService Node Mediator
                            PRIVATE
                                ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/boringssl/ssl/libssl.a
                                ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/boringssl/crypto/libcrypto.a
                                z ${CMAKE_BINARY_DIR}/src/depends/grpc/third_party/cares/cares/lib/libcares.a
                                ${CMAKE_BINARY_DIR}/src/depends/grpc/libaddress_sorting.a)
endif()