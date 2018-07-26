add_library (POW pow.cpp)
include_directories(${CMAKE_SOURCE_DIR}/src/depends/)

target_include_directories (POW PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})

target_link_libraries (POW PRIVATE ethash OpenCL ethash-cl Constants)
