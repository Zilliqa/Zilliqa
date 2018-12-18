set(OPENSSL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/openssl)
set(OPENSSL_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/openssl)
set(OPENSSL_INSTALL_DIR ${CMAKE_BINARY_DIR}/openssl)
set(OPENSSL_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_openssl.log)

file(MAKE_DIRECTORY ${OPENSSL_INSTALL_DIR})
file(MAKE_DIRECTORY ${OPENSSL_BINARY_DIR})

message(STATUS "Building and installing openssl")

include(ProcessorCount)
ProcessorCount(N)

# download, check and untar
execute_process(
    COMMAND git submodule update --init src/depends/openssl
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE OPENSSL_INSTALL_RET
    OUTPUT_FILE ${OPENSSL_INSTALL_LOG}
    ERROR_FILE ${OPENSSL_INSTALL_LOG}
)

if(NOT "${OPENSSL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing openssl, see more in log ${OPENSSL_INSTALL_LOG}")
endif()

# generate build directory
execute_process(
    #TODO: add no-shared to avoid build dynamic libraries
    COMMAND ${OPENSSL_SOURCE_DIR}/config --prefix=${OPENSSL_INSTALL_DIR}
    WORKING_DIRECTORY ${OPENSSL_BINARY_DIR}
    RESULT_VARIABLE OPENSSL_INSTALL_RET
    OUTPUT_FILE ${OPENSSL_INSTALL_LOG}
    ERROR_FILE ${OPENSSL_INSTALL_LOG}
)

if(NOT "${OPENSSL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing openssl, see more in log ${OPENSSL_INSTALL_LOG}")
endif()

# build and install openssl
execute_process(
    COMMAND make -j${N}
    WORKING_DIRECTORY ${OPENSSL_BINARY_DIR}
    RESULT_VARIABLE OPENSSL_INSTALL_RET
    OUTPUT_FILE ${OPENSSL_INSTALL_LOG}
    ERROR_FILE ${OPENSSL_INSTALL_LOG}
)

if(NOT "${OPENSSL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing openssl, see more in log ${OPENSSL_INSTALL_LOG}")
endif()

execute_process(
    COMMAND make install_sw
    WORKING_DIRECTORY ${OPENSSL_BINARY_DIR}
    RESULT_VARIABLE OPENSSL_INSTALL_RET
    OUTPUT_FILE ${OPENSSL_INSTALL_LOG}
    ERROR_FILE ${OPENSSL_INSTALL_LOG}
)

if(NOT "${OPENSSL_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing openssl, see more in log ${OPENSSL_INSTALL_LOG}")
endif()
