add_library (minilzo SHARED minilzo.c)
target_include_directories (minilzo PUBLIC ${PROJECT_SOURCE_DIR}/src)