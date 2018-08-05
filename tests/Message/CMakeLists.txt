link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_Message Test_Message.cpp)
target_include_directories (Test_Message PUBLIC ${CMAKE_BINARY_DIR}/src)
target_link_libraries(Test_Message PUBLIC Message Boost::unit_test_framework Utils)
add_test(NAME Test_Message COMMAND Test_Message)