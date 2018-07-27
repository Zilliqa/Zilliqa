add_library (POW pow.cpp)
include_directories(${CMAKE_SOURCE_DIR}/src/depends/)

target_include_directories (POW PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})

find_library(OPENCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH)
target_link_libraries (POW PRIVATE ${OPENCL_LIBRARIES} ethash ethash-cl Constants)
