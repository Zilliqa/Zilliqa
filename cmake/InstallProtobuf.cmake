set(PROTOBUF_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/protobuf/cmake)
set(PROTOBUF_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/protobuf)
set(PROTOBUF_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_protobuf.log)

message(STATUS "Building and installing Protobuf")

include(ProcessorCount)
ProcessorCount(N)

execute_process(
    COMMAND bash -c "git submodule update --init --recursive src/depends/protobuf &&
        ${CMAKE_COMMAND} -H${PROTOBUF_SOURCE_DIR} -B${PROTOBUF_BINARY_DIR} \
            -Dprotobuf_BUILD_TESTS=OFF \
            -Dprotobuf_WITH_ZLIB=OFF \
            -Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} &&
        ${CMAKE_COMMAND} --build ${PROTOBUF_BINARY_DIR} -- -j${N} &&
        ${CMAKE_COMMAND} --build ${PROTOBUF_BINARY_DIR} --target install"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE PROTOBUF_INSTALL_RET
    OUTPUT_FILE ${PROTOBUF_INSTALL_LOG}
    ERROR_FILE ${PROTOBUF_INSTALL_LOG}
)

if(NOT "${PROTOBUF_INSTALL_RET}" STREQUAL "0")
    execute_process(COMMAND tail ${PROTOBUF_INSTALL_LOG})
    message(FATAL_ERROR "Error when building and installing Protobuf, see more in log ${PROTOBUF_INSTALL_LOG}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/protobuf")

