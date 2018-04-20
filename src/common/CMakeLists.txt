add_library(Constants Constants.cpp)
include_directories(${Boost_INCLUDE_DIRS})
target_include_directories(Constants PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(Constants PUBLIC ${Boost_LIBRARIES})
