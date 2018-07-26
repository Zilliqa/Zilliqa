add_library (Database LevelDB.cpp MemoryDB.cpp OverlayDB.cpp)
target_compile_options(Database PRIVATE "-Wno-unused-parameter")
target_include_directories (Database PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Database PUBLIC Common ${LEVELDB_LIBRARIES} Utils Threads::Threads)
