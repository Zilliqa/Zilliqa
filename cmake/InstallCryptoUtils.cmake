set(CRYPTOUTILS_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_cryptoutils.log)
set(CRYPTOUTILS_INSTALL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

message(STATUS "Building and installing cryptoutils")

# update submodule to the latest commit
execute_process(
    COMMAND git submodule update --init --recursive src/depends/cryptoutils
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CRYPTOUTILS_INSTALL_RET
    OUTPUT_FILE ${CRYPTOUTILS_INSTALL_LOG}
    ERROR_FILE ${CRYPTOUTILS_INSTALL_LOG}
)

if(NOT "${CRYPTOUTILS_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing cryptoutils (1), see more in log ${CRYPTOUTILS_INSTALL_LOG}")
endif()

file(MAKE_DIRECTORY ${CRYPTOUTILS_INSTALL_INCLUDE_DIR})
