link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_RumorSpreading Test_RumorSpreading.cpp)
target_include_directories (Test_RumorSpreading PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_RumorSpreading PUBLIC Network RumorSpreading TestUtils Boost::unit_test_framework)
add_test(NAME Test_RumorSpreading COMMAND Test_RumorSpreading)
