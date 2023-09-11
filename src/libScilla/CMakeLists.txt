add_library(Scilla STATIC
    ScillaClient.cpp
    ScillaIPCServer.cpp
    ScillaUtils.cpp
    UnixDomainSocketClient.cpp
    UnixDomainSocketServer.cpp)

target_include_directories(Scilla
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(Scilla
  PUBLIC
    $<IF:$<TARGET_EXISTS:jsoncpp_lib_static>,jsoncpp_lib_static,jsoncpp_lib>
  PRIVATE
    Persistence
    Utils)
