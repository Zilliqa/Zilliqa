add_library (POW pow.cpp)
target_include_directories (POW PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries (POW PUBLIC ethash)
