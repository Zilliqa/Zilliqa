add_subdirectory (common)
add_subdirectory (libDatabase)
add_subdirectory (libTrie)
add_subdirectory (NAT)
add_subdirectory (jsonrpccpp)

if(OPENCL_MINE)
    add_subdirectory (libethash-cl)
endif()

if(CUDA_MINE)
    add_subdirectory (libethash-cuda)
endif()
