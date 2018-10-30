set(GRPC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/grpc)
set(GRPC_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/grpc)
set(GRPC_INSTALL_LOG ${CMAKE_BINARY_DIR}/install_grpc.log)

message(STATUS "Building and installing gRPC")

include(ProcessorCount)
ProcessorCount(N)

execute_process(
    COMMAND bash -c "git submodule update --init --recursive src/depends/grpc &&
        ${CMAKE_COMMAND} -H${GRPC_SOURCE_DIR} -B${GRPC_BINARY_DIR} \
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} &&
        ${CMAKE_COMMAND} --build ${GRPC_BINARY_DIR} -- -j${N} &&
        ${CMAKE_COMMAND} --build ${GRPC_BINARY_DIR} --target install"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GRPC_INSTALL_RET
    OUTPUT_FILE ${GRPC_INSTALL_LOG}
    ERROR_FILE ${GRPC_INSTALL_LOG}
)

if(NOT "${GRPC_INSTALL_RET}" STREQUAL "0")
    message(FATAL_ERROR "Error when building and installing gRPC, see more in log ${GRPC_INSTALL_LOG}")
endif()

find_program(GRPC_CPP_PLUGIN
    NAMES grpc_cpp_plugin
    HINTS "${GRPC_BINARY_DIR}"
)

# Find the gRPC libraries.
find_library(GRPC_LIBRARY
  NAMES grpc
  HINTS "${GRPC_BINARY_DIR}"
)

find_library(GRPCPP_LIBRARY
  NAMES grpc++
  HINTS "${GRPC_BINARY_DIR}"
)

find_library(GPR_LIBRARY
  NAMES gpr
  HINTS "${GRPC_BINARY_DIR}"
)

set(GRPC_LIBRARIES ${GRPCPP_LIBRARY} ${GRPC_LIBRARY} ${GPR_LIBRARY})

function(GRPC_GENERATE_CPP SRCS HDRS)
  if(NOT ARGN)
    message(SEND_ERROR "Error: GRPC_GENERATE_CPP() called without any proto files")
    return()
  endif()

  # set _protobuf_include_path.
  if(PROTOBUF_GENERATE_CPP_APPEND_PATH)
    foreach(_proto ${ARGN})
      get_filename_component(_abs_file ${_proto} ABSOLUTE)
      get_filename_component(_abs_path ${_abs_file} PATH)
      if(NOT _abs_path IN_LIST _protobuf_include_path)
        list(APPEND _protobuf_include_path -I ${_abs_path})
      endif()
    endforeach()
  else()
    set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
  if(DEFINED PROTOBUF_IMPORT_DIRS)
    foreach(_file_path ${PROTOBUF_IMPORT_DIRS})
      get_filename_component(_abs_file ${_file_path} ABSOLUTE)
      if(NOT _abs_file IN_LIST _protobuf_include_path)
        list(APPEND _protobuf_include_path -I ${_abs_file})
      endif()
    endforeach()
  endif()

  set(${SRCS})
  set(${HDRS})
  foreach(_proto ${ARGN})
    get_filename_component(_abs_file ${_proto} ABSOLUTE)
    get_filename_component(_basename ${_proto} NAME_WE)

    list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${_basename}.grpc.pb.h")
    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${_basename}.grpc.pb.cc")

    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_basename}.grpc.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/${_basename}.grpc.pb.h"
      COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
           ${_protobuf_include_path} ${_abs_file}
      DEPENDS ${_abs_file}
      COMMENT "Running gRPC C++ protocol buffer compiler on ${_proto}"
      VERBATIM)
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()
