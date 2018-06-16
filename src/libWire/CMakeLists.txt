add_library (Wire wire.cpp)
target_include_directories (Wire PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries (Wire PUBLIC Crypto Network)
