add_executable(Test_DSBlockHeader Test_DSBlockHeader.cpp)
target_include_directories(Test_DSBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_DSBlockHeader PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_DSBlockHeader COMMAND Test_DSBlockHeader)

add_executable(Test_VCBlockHeader Test_VCBlockHeader.cpp)
target_include_directories(Test_VCBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_VCBlockHeader PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_VCBlockHeader COMMAND Test_VCBlockHeader)

add_executable(Test_TxBlockHeader Test_TxBlockHeader.cpp)
target_include_directories(Test_TxBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TxBlockHeader PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_TxBlockHeader COMMAND Test_TxBlockHeader)

add_executable(Test_MicroBlockHeader Test_MicroBlockHeader.cpp)
target_include_directories(Test_MicroBlockHeader PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_MicroBlockHeader PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_MicroBlockHeader COMMAND Test_MicroBlockHeader)

add_executable(Test_DSBlock Test_DSBlock.cpp)
target_include_directories(Test_DSBlock PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_DSBlock PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_DSBlock COMMAND Test_DSBlock)

add_executable(Test_MicroBlock Test_MicroBlock.cpp)
target_include_directories(Test_MicroBlock PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_MicroBlock PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_MicroBlock COMMAND Test_MicroBlock)

add_executable(Test_TxBlock Test_TxBlock.cpp)
target_include_directories(Test_TxBlock PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TxBlock PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_TxBlock COMMAND Test_TxBlock)

add_executable(Test_VCBlock Test_VCBlock.cpp)
target_include_directories(Test_VCBlock PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_VCBlock PRIVATE Blockchain Common Boost::unit_test_framework)
add_test(NAME Test_VCBlock COMMAND Test_VCBlock)

