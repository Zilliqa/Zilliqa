add_executable(ZilliqaDaemon ZilliqaDaemon.cpp)
add_executable(ZilliqaDaemon_AWS ZilliqaDaemon_AWS.cpp)

file(COPY run.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY run_local.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

