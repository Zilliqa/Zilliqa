add_executable(Test_Trie Test_Trie.cpp)
target_include_directories(Test_Trie PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Trie LINK_PUBLIC boost_system TestUtils Trie Utils)

add_executable(Test_TxnTrie Test_TxnTrie.cpp)
target_include_directories(Test_TxnTrie PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TxnTrie LINK_PUBLIC boost_system AccountData TestUtils Trie Utils)
