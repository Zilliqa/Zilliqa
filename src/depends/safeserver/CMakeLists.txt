
find_package(MHD REQUIRED)

add_library(SafeServer safehttpserver.cpp)
add_dependencies(SafeServer jsonrpc-project)
target_include_directories (SafeServer PUBLIC ${JSONRPC_INCLUDE_DIR})
target_link_libraries(SafeServer ${JSONCPP_LINK_TARGETS} jsonrpc::server ${MHD_LIBRARY})