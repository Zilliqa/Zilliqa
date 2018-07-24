add_library(Lookup Lookup.cpp Synchronizer.cpp)
target_include_directories(Lookup PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Lookup PUBLIC AccountData Network)