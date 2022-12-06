add_library(Constants STATIC Constants.cpp)
target_include_directories(Constants PUBLIC ${PROJECT_SOURCE_DIR}/src PRIVATE ${Boost_INCLUDE_DIRS})
