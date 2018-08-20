add_library(schnorr SHARED schnorr.cpp)
include_directories(${Boost_INCLUDE_DIRS})
target_include_directories(schnorr PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(schnorr PUBLIC ${Boost_LIBRARIES})
