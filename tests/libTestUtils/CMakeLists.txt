configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
add_library(TestUtils TestUtils.cpp)
target_include_directories(TestUtils PUBLIC ${PROJECT_SOURCE_DIR}/src Crypto Boost ${CMAKE_BINARY_DIR}/src)

# To-do: Test_Transaction and Test_Block need to be updated after Predicate has been temporarily commented out