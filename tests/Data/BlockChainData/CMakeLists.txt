configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_BlockChain Test_BlockChain.cpp)
target_include_directories(Test_BlockChain PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_BlockChain PUBLIC AccountData BlockChainData TestUtils)
add_test(NAME Test_BlockChain COMMAND Test_BlockChain)
