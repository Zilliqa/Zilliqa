set(CURL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/curl)
set(CURL_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/curl)
set(CURL_INSTALL_DIR ${CMAKE_BINARY_DIR}/curl)
set(CURL_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_curl.log)

file(MAKE_DIRECTORY ${CURL_INSTALL_DIR})
file(MAKE_DIRECTORY ${CURL_BINARY_DIR})

message(STATUS "Building and installing curl")

include(ProcessorCount)
ProcessorCount(N)

# download, check and untar
execute_process(
    COMMAND git submodule update --init src/depends/curl
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CURL_INSTALL_RET
    OUTPUT_FILE ${CURL_INSTALL_LOG}
    ERROR_FILE ${CURL_INSTALL_LOG}
)

if(NOT "${CURL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing curl, see more in log ${CURL_INSTALL_LOG}")
endif()

# generate build directory
execute_process(
    #TODO: add no-shared to avoid build dynamic libraries
    COMMAND CPPFLAGS="-I${CMAKE_BINARY_DIR}/openssl/include" LDFLAGS="-L${CMAKE_BINARY_DIR}/openssl/lib" ${CURL_SOURCE_DIR}/configure --prefix=${CURL_INSTALL_DIR}
    WORKING_DIRECTORY ${CURL_BINARY_DIR}
    RESULT_VARIABLE CURL_INSTALL_RET
    OUTPUT_FILE ${CURL_INSTALL_LOG}
    ERROR_FILE ${CURL_INSTALL_LOG}
)

if(NOT "${CURL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing curl, see more in log ${CURL_INSTALL_LOG}")
endif()

# build and install curl
execute_process(
    COMMAND make
    WORKING_DIRECTORY ${CURL_BINARY_DIR}
    RESULT_VARIABLE CURL_INSTALL_RET
    OUTPUT_FILE ${CURL_INSTALL_LOG}
    ERROR_FILE ${CURL_INSTALL_LOG}
)

if(NOT "${CURL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing curl, see more in log ${CURL_INSTALL_LOG}")
endif()

execute_process(
    COMMAND make install
    WORKING_DIRECTORY ${CURL_BINARY_DIR}
    RESULT_VARIABLE CURL_INSTALL_RET
    OUTPUT_FILE ${CURL_INSTALL_LOG}
    ERROR_FILE ${CURL_INSTALL_LOG}
)

if(NOT "${CURL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing curl, see more in log ${CURL_INSTALL_LOG}")
endif()
