link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(test_download test_download.cpp)
target_include_directories (test_download PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_download PUBLIC Mediator Node Boost::unit_test_framework)
add_test(NAME test_download COMMAND test_download)
