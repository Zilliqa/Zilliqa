add_executable(zilliqad ZilliqaDaemon.cpp)

target_link_libraries (zilliqad Boost::program_options Boost::filesystem -s)

file(COPY run.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY run_local.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

