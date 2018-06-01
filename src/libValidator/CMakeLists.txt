add_library (Validator Validator.cpp)
target_include_directories (Validator PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries (Validator PUBLIC g3logger)
