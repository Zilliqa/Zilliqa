set(MONGO_C_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/mongo-c-driver)
set(MONGO_C_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/mongo-c-driver)

set(MONGO_CXX_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/mongo-cxx-driver)
set(MONGO_CXX_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/mongo-cxx-driver)

include(ProcessorCount)
ProcessorCount(N)

message(STATUS "Building and installing mongo driver. This could take longer")

set(INSTALL_MONGO_LOG ${CMAKE_BINARY_DIR}/install_mongo.log)

# generate build directory
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -H${MONGO_C_SOURCE_DIR}
        -B${MONGO_C_BINARY_DIR}
        -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    RESULT_VARIABLE INSTALL_MONGO_RET
    OUTPUT_FILE ${INSTALL_MONGO_LOG}
    ERROR_FILE ${INSTALL_MONGO_LOG}
)

if(NOT "${INSTALL_MONGO_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing mongo, see more in log ${INSTALL_MONGO_LOG}")
endif()

# build and install mongo-c-driver
execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${MONGO_C_BINARY_DIR} -- -j${N}
    RESULT_VARIABLE INSTALL_MONGO_RET
    OUTPUT_FILE ${INSTALL_MONGO_LOG}
    ERROR_FILE ${INSTALL_MONGO_LOG}
)

if(NOT "${INSTALL_MONGO_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing mongo, see more in log ${INSTALL_MONGO_LOG}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${MONGO_C_BINARY_DIR} --target install
    RESULT_VARIABLE INSTALL_MONGO_RET
    OUTPUT_FILE ${INSTALL_MONGO_LOG}
    ERROR_FILE ${INSTALL_MONGO_LOG}
)

if(NOT "${INSTALL_MONGO_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing mongo, see more in log ${INSTALL_MONGO_LOG}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/libmongoc-1.0")

# generate build directory
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -H${MONGO_CXX_SOURCE_DIR}
        -B${MONGO_CXX_BINARY_DIR}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    RESULT_VARIABLE INSTALL_MONGO_RET
    OUTPUT_FILE ${INSTALL_MONGO_LOG}
    ERROR_FILE ${INSTALL_MONGO_LOG}
)

if(NOT "${INSTALL_MONGO_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing mongo, see more in log ${INSTALL_MONGO_LOG}")
endif()

# build and install mongo-cxx-driver
execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${MONGO_CXX_BINARY_DIR} -- -j${N}
    RESULT_VARIABLE INSTALL_MONGO_RET
    OUTPUT_FILE ${INSTALL_MONGO_LOG}
    ERROR_FILE ${INSTALL_MONGO_LOG}
)

if(NOT "${INSTALL_MONGO_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing mongo, see more in log ${INSTALL_MONGO_LOG}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${MONGO_CXX_BINARY_DIR} --target install
    RESULT_VARIABLE INSTALL_MONGO_RET
    OUTPUT_FILE ${INSTALL_MONGO_LOG}
    ERROR_FILE ${INSTALL_MONGO_LOG}
)

if(NOT "${INSTALL_MONGO_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing mongo, see more in log ${INSTALL_MONGO_LOG}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/libbsoncxx-3.3.1")
