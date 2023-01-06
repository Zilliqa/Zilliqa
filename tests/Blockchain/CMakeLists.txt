add_executable(Test_DSBlockHeader Test_DSBlockHeader.cpp)
target_include_directories(Test_DSBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_DSBlockHeader PRIVATE BlockHeader Message Boost::unit_test_framework)
add_test(NAME Test_DSBlockHeader COMMAND Test_DSBlockHeader)

add_executable(Test_VCBlockHeader Test_VCBlockHeader.cpp)
target_include_directories(Test_VCBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_VCBlockHeader PRIVATE BlockHeader Message Boost::unit_test_framework)
add_test(NAME Test_VCBlockHeader COMMAND Test_VCBlockHeader)

add_executable(Test_TxBlockHeader Test_TxBlockHeader.cpp)
target_include_directories(Test_TxBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TxBlockHeader PRIVATE BlockHeader Message Boost::unit_test_framework)
add_test(NAME Test_TxBlockHeader COMMAND Test_TxBlockHeader)

