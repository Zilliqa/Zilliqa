add_executable(zilliqad ZilliqaDaemon.cpp ZilliqaUpdater.cpp)

target_link_libraries (zilliqad PRIVATE Constants Boost::program_options Boost::filesystem jsoncpp_lib Boost::iostreams)

file(COPY run.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY run_local.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

