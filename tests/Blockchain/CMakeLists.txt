add_executable(Test_BlockHeaders Test_BlockHeaders.cpp)
target_include_directories(Test_BlockHeaders PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_BlockHeaders PUBLIC Blockchain TestUtils)
add_test(NAME Test_BlockHeaders COMMAND Test_BlockHeaders)

