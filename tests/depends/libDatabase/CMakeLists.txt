add_executable(Test_LevelDB Test_LevelDB.cpp)
target_include_directories(Test_LevelDB PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_LevelDB LINK_PUBLIC ${Boost_LIBRARIES}  Database Utils)