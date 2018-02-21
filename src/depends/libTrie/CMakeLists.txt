add_library (Trie SHARED Trie.cpp TrieCommon.cpp TrieHash.cpp)
target_include_directories (Trie PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Trie LINK_PUBLIC Common Database ${LevelDB_LIBRARIES} Utils)