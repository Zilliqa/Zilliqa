link_directories(${CMAKE_BINARY_DIR}/lib)

# TODO: This UnitTest used to be disabled, try to enable those commented parts by fixing the compilation errors
add_executable(Test_Trie Test_Trie.cpp)
target_include_directories(Test_Trie PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Trie PUBLIC Trie Utils Constants)

# add_executable(Test_TxnTrie Test_TxnTrie.cpp)
# target_include_directories(Test_TxnTrie PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
# target_link_libraries(Test_TxnTrie PUBLIC boost_system AccountData TestUtils Trie Utils)

configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
add_executable(Test_TriePerformance Test_TriePerformance.cpp)
target_include_directories(Test_TriePerformance PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TriePerformance PUBLIC Trie Utils Constants)
add_test(NAME Test_TriePerformance COMMAND Test_TriePerformance)
