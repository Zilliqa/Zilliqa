configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
add_library(TestUtils TestUtils.cpp)
target_include_directories(TestUtils PUBLIC ${PROJECT_SOURCE_DIR}/src Boost ${CMAKE_BINARY_DIR}/src)
target_link_libraries (TestUtils PUBLIC Message BlockHeader)

# To-do: Test_Transaction and Test_Block need to be updated after Predicate has been temporarily commented out