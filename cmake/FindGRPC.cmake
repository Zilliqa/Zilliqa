# Find gRPC
#

# Find the cpp plugin.
find_program(GRPC_CPP_PLUGIN
  NAMES grpc_cpp_plugin
  HINTS "/tmp/grpc/bins/opt"
)

# Find the gRPC libraries.
set(GRPC_LIB_PATH "/tmp/grpc/libs/opt/")
find_library(GRPC_LIBRARY
  NAMES grpc
  HINTS ${GRPC_LIB_PATH}
)
find_library(GRPCPP_LIBRARY
  NAMES grpc++
  HINTS ${GRPC_LIB_PATH}
)
find_library(GPR_LIBRARY
  NAMES gpr
  HINTS ${GRPC_LIB_PATH}
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
