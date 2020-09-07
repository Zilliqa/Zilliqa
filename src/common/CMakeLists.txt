add_library(Constants Constants.cpp TxnStatus.cpp)
include_directories(${Boost_INCLUDE_DIRS})
target_include_directories(Constants PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Constants PUBLIC ${Boost_LIBRARIES})
