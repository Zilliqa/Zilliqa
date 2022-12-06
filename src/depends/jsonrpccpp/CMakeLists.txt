# HTTP client from JSON RPC CPP requires curl library. It can find it itself,
# but we need to know the libcurl location for static linking.
find_package(CURL REQUIRED)

# only what is needed here
add_library(jsonrpc STATIC
    common/errors.cpp
    common/exception.cpp
    common/procedure.cpp
    client/batchcall.cpp
    client/batchresponse.cpp
    client/client.cpp
    client/rpcprotocolclient.cpp
    client/connectors/httpclient.cpp
    server/abstractprotocolhandler.cpp
    server/abstractserverconnector.cpp
    server/requesthandlerfactory.cpp
    server/rpcprotocolserver12.cpp
    server/rpcprotocolserverv1.cpp
    server/rpcprotocolserverv2.cpp
    )

target_link_libraries(jsonrpc CURL::libcurl)
set_property(TARGET jsonrpc PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
