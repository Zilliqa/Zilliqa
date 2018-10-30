configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

#link_directories(${CMAKE_BINARY_DIR}/lib)


add_library(testLib testLibDataGenerators.cpp)
target_include_directories(testLib PUBLIC ${PROJECT_SOURCE_DIR}/src Crypto Boost ${G3LOG_INCLUDE_DIRS} ${CMAKE_BINARY_DIR}/src)

# To-do: Test_Transaction and Test_Block need to be updated after Predicate has been temporarily commented out
