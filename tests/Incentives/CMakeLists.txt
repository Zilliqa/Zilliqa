link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_coinbase test_coinbase.cpp)
target_include_directories(Test_coinbase PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_coinbase PUBLIC Utils Boost::unit_test_framework Network Zilliqa  TestUtils Validator DirectoryService)
add_test(NAME Test_coinbase COMMAND Test_coinbase)

