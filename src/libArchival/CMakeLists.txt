
add_library (Archival BaseDB.cpp ArchiveDB.cpp ExplorerDB.cpp Archival.cpp)

target_include_directories (Archival PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(Archival PUBLIC ${LIBMONGOCXX_LIBRARIES})
target_link_libraries(Archival PUBLIC ${LIBBSONCXX_LIBRARIES})
target_link_libraries(Archival PUBLIC Server)

