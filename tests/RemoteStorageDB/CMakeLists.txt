configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_Mongo Test_mongo.cpp)
target_include_directories(Test_Mongo PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Mongo PUBLIC RemoteStorageDB TestUtils AccountData)
add_test(NAME Test_Mongo COMMAND Test_Mongo)