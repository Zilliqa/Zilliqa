configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

link_directories(${CMAKE_BINARY_DIR}/lib)
add_executable(Test_Setup Test_Setup.cpp)
target_include_directories(Test_Setup PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Setup PUBLIC ${Boost_LIBRARIES} Constants Utils)
add_test(NAME Test_Setup COMMAND Test_Setup)