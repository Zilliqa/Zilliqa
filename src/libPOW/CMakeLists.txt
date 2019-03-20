add_library (POW pow.cpp)

include_directories(${CMAKE_SOURCE_DIR}/src/depends/)
add_dependencies(POW jsonrpc-project)
target_include_directories (POW PUBLIC ${PROJECT_SOURCE_DIR}/src ${JSONRPC_INCLUDE_DIR})
target_link_libraries (POW PRIVATE ethash Constants Common Server ${JSONCPP_LINK_TARGETS} jsonrpc::client)

if(OPENCL_MINE)
    find_library(OPENCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH)
    target_link_libraries(POW PRIVATE ${OPENCL_LIBRARIES} ethash-cl)
endif()

if(CUDA_MINE)
    target_link_libraries(POW PRIVATE ethash-cuda)
endif()
