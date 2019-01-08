link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(getnetworkhistory getnetworkhistory.cpp)
add_custom_command(TARGET getnetworkhistory
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:getnetworkhistory> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(getnetworkhistory PUBLIC ${CMAKE_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(getnetworkhistory PUBLIC Persistence)
