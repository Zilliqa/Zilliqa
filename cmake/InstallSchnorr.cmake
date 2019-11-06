set(SCHNORR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/Schnorr)
set(SCHNORR_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/Schnorr)
set(SCHNORR_INSTALL_DIR ${CMAKE_BINARY_DIR})
set(SCHNORR_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_Schnorr.log)

file(MAKE_DIRECTORY ${SCHNORR_BINARY_DIR})

message(STATUS "Building and installing Schnorr")

# download, check and untar
execute_process(
    COMMAND git submodule update --init --recursive --remote src/depends/Schnorr
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE SCHNORR_INSTALL_RET
    OUTPUT_FILE ${SCHNORR_INSTALL_LOG}
    ERROR_FILE ${SCHNORR_INSTALL_LOG}
)

if(NOT "${SCHNORR_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing Schnorr (1), see more in log ${SCHNORR_INSTALL_LOG}")
endif()

# generate build directory
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -H${SCHNORR_SOURCE_DIR}
        -B${SCHNORR_BINARY_DIR}
        -DCMAKE_INSTALL_PREFIX=${SCHNORR_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE:STRING=Release
        -Wno-dev
    RESULT_VARIABLE SCHNORR_INSTALL_RET
    OUTPUT_FILE ${SCHNORR_INSTALL_LOG}
    ERROR_FILE ${SCHNORR_INSTALL_LOG}
)

if(NOT "${SCHNORR_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing Schnorr (2), see more in log ${SCHNORR_INSTALL_LOG}")
endif()

# build and install proto
execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${SCHNORR_BINARY_DIR} -- -j${N}
    RESULT_VARIABLE SCHNORR_INSTALL_RET
    OUTPUT_FILE ${SCHNORR_INSTALL_LOG}
    ERROR_FILE ${SCHNORR_INSTALL_LOG}
)

if(NOT "${SCHNORR_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing Schnorr (3), see more in log ${SCHNORR_INSTALL_LOG}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${SCHNORR_BINARY_DIR} --target install
    RESULT_VARIABLE SCHNORR_INSTALL_RET
    OUTPUT_FILE ${SCHNORR_INSTALL_LOG}
    ERROR_FILE ${SCHNORR_INSTALL_LOG}
)

if(NOT "${SCHNORR_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing Schnorr (4), see more in log ${SCHNORR_INSTALL_LOG}")
endif()

list(APPEND CMAKE_PREFIX_PATH ${SCHNORR_INSTALL_DIR})
link_directories(${SCHNORR_INSTALL_DIR}/lib)

find_path(
    SCHNORR_INCLUDE_DIR
    NAMES include/Schnorr.h
    DOC "Schnorr include directory"
)
