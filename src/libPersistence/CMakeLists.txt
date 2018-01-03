add_library (Persistence BlockStorage.cpp DB.cpp)
target_include_directories (Persistence PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Persistence LINK_PUBLIC AccountData Crypto ${LevelDB_LIBRARIES} minilzo Trie Utils)
