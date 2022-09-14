set(PROTOBUF_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/protobuf/cmake)
set(PROTOBUF_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/protobuf)
set(PROTOBUF_INSTALL_DIR ${CMAKE_BINARY_DIR}/protobuf)
set(PROTOBUF_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_protobuf.log)

file(MAKE_DIRECTORY ${PROTOBUF_INSTALL_DIR})
file(MAKE_DIRECTORY ${PROTOBUF_BINARY_DIR})

message(STATUS "Building and installing protobuf")

# download, check and untar
execute_process(
    COMMAND git submodule update --init src/depends/protobuf
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE PROTOBUF_INSTALL_RET
    OUTPUT_FILE ${PROTOBUF_INSTALL_LOG}
    ERROR_FILE ${PROTOBUF_INSTALL_LOG}
)

if(NOT "${PROTOBUF_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing protobuf (1), see more in log ${PROTOBUF_INSTALL_LOG}")
endif()

set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build protobuf tests")
set(protobuf_WITH_ZLIB OFF CACHE BOOL "Build protobuf with zlib.")
set(protobuf_MODULE_COMPATIBLE ON CACHE BOOL "")

# generate build directory
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -H${PROTOBUF_SOURCE_DIR}
        -B${PROTOBUF_BINARY_DIR}
        -Dprotobuf_BUILD_TESTS:BOOL=OFF
        -Dprotobuf_WITH_ZLIB:BOOL=OFF
        -Dprotobuf_MSVC_STATIC_RUNTIME:BOOL=OFF
        -Dprotobuf_MODULE_COMPATIBLE:BOOL=ON
        -DCMAKE_INSTALL_PREFIX=${PROTOBUF_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE:STRING=Release
        -Wno-dev
    RESULT_VARIABLE PROTOBUF_INSTALL_RET
    OUTPUT_FILE ${PROTOBUF_INSTALL_LOG}
    ERROR_FILE ${PROTOBUF_INSTALL_LOG}
)

if(NOT "${PROTOBUF_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing protobuf (2), see more in log ${PROTOBUF_INSTALL_LOG}")
endif()

# build and install proto
execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${PROTOBUF_BINARY_DIR} -- -j${N}
    RESULT_VARIABLE PROTOBUF_INSTALL_RET
    OUTPUT_FILE ${PROTOBUF_INSTALL_LOG}
    ERROR_FILE ${PROTOBUF_INSTALL_LOG}
)

if(NOT "${PROTOBUF_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing protobuf (3), see more in log ${PROTOBUF_INSTALL_LOG}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${PROTOBUF_BINARY_DIR} --target install
    RESULT_VARIABLE PROTOBUF_INSTALL_RET
    OUTPUT_FILE ${PROTOBUF_INSTALL_LOG}
    ERROR_FILE ${PROTOBUF_INSTALL_LOG}
)

if(NOT "${PROTOBUF_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing protobuf (4), see more in log ${PROTOBUF_INSTALL_LOG}")
endif()

list(APPEND CMAKE_PREFIX_PATH ${PROTOBUF_INSTALL_DIR})
list(APPEND CMAKE_MODULE_PATH "${PROTOBUF_INSTALL_DIR}/lib/cmake/protobuf")
link_directories(${PROTOBUF_INSTALL_DIR}/lib)
