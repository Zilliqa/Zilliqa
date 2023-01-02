add_library(Scilla STATIC
    ScillaIPCServer.cpp
    ScillaUtils.cpp
    UnixDomainSocketClient.cpp
    UnixDomainSocketServer.cpp)

target_include_directories(Scilla
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(Scilla
  PUBLIC
    jsoncpp_lib
  PRIVATE
    Persistence)
