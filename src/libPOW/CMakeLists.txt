add_library (POW pow.cpp)
target_include_directories (POW PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (POW PUBLIC ethash)
