add_library (Validator Validator.cpp)
target_include_directories (Validator PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Validator PUBLIC Mediator g3logger)
