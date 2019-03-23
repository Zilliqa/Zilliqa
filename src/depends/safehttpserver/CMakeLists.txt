
add_library(SafeHttp safehttpserver.cpp)
target_link_libraries(SafeHttp ${JSONCPP_LINK_TARGETS} ${JSONRPCCPP_LINK_TARGETS} ${MHD_LIBRARY})