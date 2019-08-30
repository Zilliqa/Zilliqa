
add_executable(test_states test_states.cpp)
target_include_directories(test_states PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_states PUBLIC Node Mediator TestUtils Validator)
add_test(NAME test_states COMMAND test_states)

add_custom_command(TARGET test_states PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/config $<TARGET_FILE_DIR:test_states>)

configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_custom_command(TARGET test_states POST_BUILD
        COMMAND sed -i '/<LOOKUP_NODE_MODE>/c\        <LOOKUP_NODE_MODE>true</LOOKUP_NODE_MODE>' constants.xml)

add_custom_command(TARGET test_states POST_BUILD
        COMMAND echo "<nodes></nodes>" > config.xml VERBATIM)

