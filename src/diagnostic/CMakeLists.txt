link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(getnetworkhistory getnetworkhistory.cpp)
add_executable(getrewardhistory getrewardhistory.cpp)

add_custom_command(TARGET getnetworkhistory
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:getnetworkhistory> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(getnetworkhistory PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(getnetworkhistory PUBLIC Persistence -s)

add_custom_command(TARGET getrewardhistory
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:getrewardhistory> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(getrewardhistory PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(getrewardhistory PUBLIC Persistence -s)
