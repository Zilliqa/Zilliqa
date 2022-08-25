find_package(Miniupnpc REQUIRED)

add_library (NAT SHARED nat.cpp)
target_include_directories(NAT PUBLIC ${PROJECT_SOURCE_DIR}/src ${MINIUPNPC_INCLUDE_DIR})
target_link_libraries(NAT ${MINIUPNPC_LIBRARY}) 

