add_library (RemoteStorageDB  RemoteStorageDB.cpp)

target_include_directories (RemoteStorageDB PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(RemoteStorageDB PRIVATE mongo::mongocxx_shared g3log Utils)
