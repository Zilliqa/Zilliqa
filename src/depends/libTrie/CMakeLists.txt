link_directories(${CMAKE_BINARY_DIR}/lib)
add_library (Trie SHARED Trie.cpp TrieCommon.cpp TrieHash.cpp)
target_include_directories (Trie PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Trie PUBLIC Common Database ${LevelDB_LIBRARIES} ${SNAPPY_LIBRARIES} Utils)
