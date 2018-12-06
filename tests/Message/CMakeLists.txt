link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_Messenger_Primitives Test_Messenger_Primitives.cpp)
target_include_directories (Test_Messenger_Primitives PUBLIC ${CMAKE_BINARY_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Messenger_Primitives PUBLIC AccountData Message Boost::unit_test_framework Utils TestUtils)
add_test(NAME Test_Messenger_Primitives COMMAND Test_Messenger_Primitives)

add_executable(Test_Messenger_Consensus Test_Messenger_Consensus.cpp)
target_include_directories (Test_Messenger_Consensus PUBLIC ${CMAKE_BINARY_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Messenger_Consensus PUBLIC AccountData Message Boost::unit_test_framework Utils TestUtils)
add_test(NAME Test_Messenger_Consensus COMMAND Test_Messenger_Consensus)

add_executable(Test_MessageName Test_MessageName.cpp)
target_include_directories (Test_MessageName PUBLIC ${CMAKE_BINARY_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_MessageName PUBLIC Zilliqa Validator Boost::unit_test_framework Utils)
add_test(NAME Test_MessageName COMMAND Test_MessageName)
