add_executable(zilliqad ZilliqaDaemon.cpp ZilliqaUpdater.cpp)

target_link_libraries (zilliqad PRIVATE Boost::program_options jsoncpp_lib Updater)

file(COPY run.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY run_local.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

