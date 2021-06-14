add_library(TraceableDB TraceableDB.cpp)
target_compile_options(TraceableDB PRIVATE "-Wno-unused-parameter")
target_include_directories(TraceableDB PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (TraceableDB PUBLIC Database)