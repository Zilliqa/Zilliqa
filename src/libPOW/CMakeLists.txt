add_library (POW pow.cpp)

target_include_directories (POW PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (POW PRIVATE Constants Common jsonrpc)

if(OPENCL_MINE)
    find_library(OPENCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH)
    target_link_libraries(POW PRIVATE ${OPENCL_LIBRARIES} ethash-cl)
endif()

if(CUDA_MINE)
    target_link_libraries(POW PRIVATE ethash-cuda)
endif()
