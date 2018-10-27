link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_Messenger_Primitives Test_Messenger_Primitives.cpp)
target_include_directories (Test_Messenger_Primitives PUBLIC ${CMAKE_BINARY_DIR}/src)
target_link_libraries(Test_Messenger_Primitives PUBLIC AccountData Message Boost::unit_test_framework Utils)
add_test(NAME Test_Messenger_Primitives COMMAND Test_Messenger_Primitives)