add_library (NAT SHARED nat.cpp)
target_include_directories(NAT PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(NAT miniupnpc) 

