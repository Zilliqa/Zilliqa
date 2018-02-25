add_library(Server Server.cpp)
target_include_directories(Server PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Server LINK_PUBLIC AccountData BlockChainData jsoncpp jsonrpccpp-common jsonrpccpp-server)