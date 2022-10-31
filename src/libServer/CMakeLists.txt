add_library(Server
    Server.cpp
    ScillaIPCServer.cpp
    JSONConversion.cpp
    GetWorkServer.cpp
    LookupServer.cpp
    StakingServer.cpp
    StatusServer.cpp
    WebsocketServer.cpp
    IsolatedServer.cpp
    EthRpcMethods.h
    EthRpcMethods.cpp
    APIServerImpl.cpp
    APIThreadPool.cpp
    WebsocketServerImpl.cpp)

add_dependencies(Server jsonrpc-project)

target_include_directories(Server PUBLIC 
                                  ${PROJECT_SOURCE_DIR}/src)

target_link_libraries (Server PUBLIC
                              jsonrpc::server
                              AccountData
                              Validator
                              Eth)

target_link_libraries (Server PRIVATE 
                              SafeServer)
