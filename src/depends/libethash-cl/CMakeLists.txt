# A custom command and target to turn the OpenCL kernel into a byte array header
# The normal build depends on it properly and if the kernel file is changed, then
# a rebuild of libethash-cl should be triggered

#TODO: clean up the copy&pasting here
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_stable.h
	COMMAND ${CMAKE_COMMAND} ARGS
	-DBIN2H_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel_stable.cl"
	-DBIN2H_VARIABLE_NAME=CLMiner_kernel_stable
	-DBIN2H_HEADER_FILE="${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_stable.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake"
	COMMENT "Generating OpenCL Kernel Byte Array"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel_stable.cl
)
add_custom_target(clbin2h_stable DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_stable.h ${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel_stable.cl)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_experimental.h
	COMMAND ${CMAKE_COMMAND} ARGS
	-DBIN2H_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel_experimental.cl"
	-DBIN2H_VARIABLE_NAME=CLMiner_kernel_experimental
	-DBIN2H_HEADER_FILE="${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_experimental.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake"
	COMMENT "Generating OpenCL Kernel Byte Array"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel_experimental.cl
)
add_custom_target(clbin2h_experimental DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_experimental.h ${CMAKE_CURRENT_SOURCE_DIR}/CLMiner_kernel_experimental.cl)

set(SOURCES
	CLMiner.h CLMiner.cpp
	${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_stable.h
	${CMAKE_CURRENT_BINARY_DIR}/CLMiner_kernel_experimental.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(..)

find_library(OPENCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH)

add_library(ethash-cl SHARED ${SOURCES})
target_link_libraries(ethash-cl PUBLIC Common ${CryptoUtils_LIBRARY})
target_link_libraries(ethash-cl PRIVATE ${OPENCL_LIBRARIES})
