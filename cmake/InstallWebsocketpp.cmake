set(WEBSOCKETPP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/websocketpp)
set(WEBSOCKETPP_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/websocketpp)
set(WEBSOCKETPP_INSTALL_DIR ${CMAKE_BINARY_DIR}/websocketpp)
set(WEBSOCKETPP_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_websocketpp.log)

file(MAKE_DIRECTORY ${WEBSOCKETPP_INSTALL_DIR})
file(MAKE_DIRECTORY ${WEBSOCKETPP_BINARY_DIR})

message(STATUS "Building and installing websocketpp")

# update submodule to the latest commit
execute_process(
    COMMAND git submodule update --init --recursive --remote src/depends/websocketpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE WEBSOCKETPP_INSTALL_RET
    OUTPUT_FILE ${WEBSOCKETPP_INSTALL_LOG}
    ERROR_FILE ${WEBSOCKETPP_INSTALL_LOG}
)

if(NOT "${WEBSOCKETPP_INSTALL_RET}" STREQUAL "0")
	message(FATAL_ERROR "Error when building and installing websocketpp (1), see more in log ${WEBSOCKETPP_INSTALL_LOG}")
endif()

# generate build directory
execute_process(
	COMMAND ${CMAKE_COMMAND}
	    -H${WEBSOCKETPP_SOURCE_DIR}
	    -B${WEBSOCKETPP_BINARY_DIR}
	    -DCMAKE_INSTALL_PREFIX=${WEBSOCKETPP_INSTALL_DIR}
	    -Wno-dev
	RESULT_VARIABLE WEBSOCKETPP_INSTALL_RET
	OUTPUT_FILE ${WEBSOCKETPP_INSTALL_LOG}
	ERROR_FILE ${WEBSOCKETPP_INSTALL_LOG}
)

if(NOT "${WEBSOCKETPP_INSTALL_RET}" STREQUAL "0")
	message(FATAL_ERROR "Error when building and installing websocketpp (2), see more in log ${WEBSOCKETPP_INSTALL_LOG}")
endif()

# build and install websocketpp
execute_process(
	COMMAND ${CMAKE_COMMAND} --build ${WEBSOCKETPP_BINARY_DIR} -- 
	RESULT_VARIABLE WEBSOCKETPP_INSTALL_RET
	OUTPUT_FILE ${WEBSOCKETPP_INSTALL_LOG}
	ERROR_FILE ${WEBSOCKETPP_INSTALL_LOG}
)

if(NOT "${WEBSOCKETPP_INSTALL_RET}" STREQUAL "0")
	message(FATAL_ERROR "Error when building and installing websocketpp (3), see more in log ${WEBSOCKETPP_INSTALL_LOG}")
endif()

execute_process(
	COMMAND ${CMAKE_COMMAND} --build ${WEBSOCKETPP_BINARY_DIR} --target install
	RESULT_VARIABLE WEBSOCKETPP_INSTALL_RET
	OUTPUT_FILE ${WEBSOCKETPP_INSTALL_LOG}
	ERROR_FILE ${WEBSOCKETPP_INSTALL_LOG}
)

if(NOT "${WEBSOCKETPP_INSTALL_RET}" STREQUAL "0")
	message(FATAL_ERROR "Error when building and installing websocketpp (3), see more in log ${WEBSOCKETPP_INSTALL_LOG}")
endif()

list(APPEND CMAKE_PREFIX_PATH ${WEBSOCKETPP_INSTALL_DIR})
list(APPEND CMAKE_MODULE_PATH "${WEBSOCKETPP_INSTALL_DIR}/lib/cmake/websocketpp")
link_directories(${WEBSOCKETPP_INSTALL_DIR}/lib)