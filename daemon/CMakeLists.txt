add_executable(zilliqad ZilliqaDaemon.cpp ZilliqaUpdater.cpp)

target_include_directories(zilliqad PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (zilliqad PRIVATE Boost::program_options jsoncpp_lib g3log Constants OpenSSL::Crypto Utils Updater)

file(COPY run.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY run_local.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

