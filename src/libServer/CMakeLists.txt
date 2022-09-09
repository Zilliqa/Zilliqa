add_library(Server Server.cpp ScillaIPCServer.cpp JSONConversion.cpp GetWorkServer.cpp LookupServer.cpp StakingServer.cpp StatusServer.cpp WebsocketServer.cpp IsolatedServer.cpp)

add_dependencies(Server jsonrpc-project)

target_include_directories(Server PRIVATE
                                  ${OPENTELEMETRY_CPP_INCLUDE_DIRS}
                                  PUBLIC
                                  ${PROJECT_SOURCE_DIR}/src 
                                  ${JSONRPC_INCLUDE_DIR} 
                                  ${WEBSOCKETPP_INCLUDE_DIR})

target_link_libraries (Server PRIVATE
                              opentelemetry-cpp::sdk
                              opentelemetry-cpp::ext
                              opentelemetry-cpp::api
                              opentelemetry-cpp::logs
                              PUBLIC
                              AccountData 
                              Validator 
                              RemoteStorageDB 
                              Eth 
                              ${JSONCPP_LINK_TARGETS}
                              ${WEBSOCKETPP_LIB}
                              Utils
                              Constants 
                              POW)

target_link_libraries (Server PRIVATE 
                              CryptoUtils 
                              SafeServer)
