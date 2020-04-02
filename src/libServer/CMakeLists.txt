add_library(Server Server.cpp ScillaIPCServer.cpp JSONConversion.cpp GetWorkServer.cpp LookupServer.cpp StakingServer.cpp StatusServer.cpp IsolatedServer.cpp WebsocketServer.cpp)

add_dependencies(Server jsonrpc-project)
target_include_directories(Server PUBLIC ${PROJECT_SOURCE_DIR}/src ${JSONRPC_INCLUDE_DIR} ${WEBSOCKETPP_INCLUDE_DIR})
target_link_libraries (Server PUBLIC AccountData Validator ${JSONCPP_LINK_TARGETS} ${WEBSOCKETPP_LIB} Utils Constants)
target_link_libraries (Server PRIVATE ethash SafeServer)

