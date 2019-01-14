set(G3LOG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/g3log)
set(G3LOG_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/g3log)
set(G3LOG_INSTALL_DIR ${CMAKE_BINARY_DIR}/g3log)
set(G3LOG_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_g3log.log)

message(STATUS "Building and installing g3log")

include(ProcessorCount)
ProcessorCount(N)

# update submodule to the latest commit
execute_process(
    COMMAND git submodule update --init --recursive --remote src/depends/g3log
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE G3LOG_INSTALL_RET
    OUTPUT_FILE ${G3LOG_INSTALL_LOG}
    ERROR_FILE ${G3LOG_INSTALL_LOG}
)

if(NOT "${G3LOG_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing g3log, see more in log ${G3LOG_INSTALL_LOG}")
endif()


# generate build directory
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -H${G3LOG_SOURCE_DIR}
        -B${G3LOG_BINARY_DIR}
        -DUSE_DYNAMIC_LOGGING_LEVELS=ON
        -DG3_SHARED_LIB=OFF
        -DCMAKE_INSTALL_PREFIX=${G3LOG_INSTALL_DIR}
        -DADD_FATAL_EXAMPLE=OFF
        -DENABLE_FATAL_SIGNALHANDLING=OFF
        -Wno-dev
    RESULT_VARIABLE G3LOG_INSTALL_RET
    OUTPUT_FILE ${G3LOG_INSTALL_LOG}
    ERROR_FILE ${G3LOG_INSTALL_LOG}
)

if(NOT "${G3LOG_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing g3log, see more in log ${G3LOG_INSTALL_LOG}")
endif()

# build and install g3log
execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${G3LOG_BINARY_DIR} -- -j${N}
    RESULT_VARIABLE G3LOG_INSTALL_RET
    OUTPUT_FILE ${G3LOG_INSTALL_LOG}
    ERROR_FILE ${G3LOG_INSTALL_LOG}
)

if(NOT "${G3LOG_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing g3log, see more in log ${G3LOG_INSTALL_LOG}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${G3LOG_BINARY_DIR} --target install
    RESULT_VARIABLE G3LOG_INSTALL_RET
    OUTPUT_FILE ${G3LOG_INSTALL_LOG}
    ERROR_FILE ${G3LOG_INSTALL_LOG}
)

if(NOT "${G3LOG_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing g3log, see more in log ${G3LOG_INSTALL_LOG}")
endif()

list(APPEND CMAKE_PREFIX_PATH ${G3LOG_INSTALL_DIR})
list(APPEND CMAKE_MODULE_PATH "${G3LOG_INSTALL_DIR}/lib/cmake/g3logger")
link_directories(${G3LOG_INSTALL_DIR}/lib)
