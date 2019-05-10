add_library(Server Server.cpp JSONConversion.cpp GetWorkServer.cpp LookupServer.cpp StatusServer.cpp)

add_dependencies(Server jsonrpc-project)
target_include_directories(Server PUBLIC ${PROJECT_SOURCE_DIR}/src ${JSONRPC_INCLUDE_DIR})
target_link_libraries (Server PUBLIC AccountData ${JSONCPP_LINK_TARGETS})
target_link_libraries (Server PRIVATE ethash SafeServer)

