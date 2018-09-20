find_package(libmongocxx REQUIRED)
find_package(libbsoncxx REQUIRED)
include_directories(${LIBMONGOCXX_INCLUDE_DIR})
include_directories(${LIBBSONCXX_INCLUDE_DIR})
include_directories("/usr/local/include/mongocxx/v_noabi")
include_directories("/usr/local/include/bsoncxx/v_noabi")
include_directories("/usr/local/include/libmongoc-1.0")
include_directories("/usr/local/include/libbson-1.0")
include_directories("/usr/local/lib")


add_library (DB BaseDB.cpp ArchiveDB.cpp ExplorerDB.cpp Archival.cpp)

target_include_directories (DB PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(DB PUBLIC ${LIBMONGOCXX_LIBRARIES})
target_link_libraries(DB PUBLIC ${LIBBSONCXX_LIBRARIES})
target_link_libraries(DB PUBLIC Server Data)

