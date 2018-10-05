
add_library (DB BaseDB.cpp ArchiveDB.cpp ExplorerDB.cpp Archival.cpp)

target_include_directories (DB PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(DB PUBLIC ${LIBMONGOCXX_LIBRARIES})
target_link_libraries(DB PUBLIC ${LIBBSONCXX_LIBRARIES})
target_link_libraries(DB PUBLIC Server)

