

##File source: https://github.com/ethereum/cpp-dependencies/blob/master/jsonrpc.cmake


# HTTP client from JSON RPC CPP requires curl library. It can find it itself,
# but we need to know the libcurl location for static linking.
find_package(CURL REQUIRED)

# HTTP server from JSON RPC CPP requires microhttpd library. It can find it itself,
# but we need to know the MHD location for static linking.
find_package(MHD REQUIRED)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

##Supress deprecated warning
 
set(JSONRPC_CXX_FLAGS "-Wno-deprecated") 

set(CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
               -DCMAKE_BUILD_TYPE=Release
               # Build static lib but suitable to be included in a shared lib.
               -DCMAKE_POSITION_INDEPENDENT_CODE=On
               -DBUILD_STATIC_LIBS=On
               -DBUILD_SHARED_LIBS=Off
               -DUNIX_DOMAIN_SOCKET_SERVER=On
               -DUNIX_DOMAIN_SOCKET_CLIENT=On
               -DHTTP_SERVER=On
               -DHTTP_CLIENT=On
               -DCOMPILE_TESTS=Off
               -DREDIS_SERVER=NO
               -DREDIS_CLIENT=NO
               -DTCP_SOCKET_SERVER=On
               -DCOMPILE_STUBGEN=Off
               -DCOMPILE_EXAMPLES=Off
               -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
               -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}
               -DMHD_INCLUDE_DIR=${MHD_INCLUDE_DIR}
               -DMHD_LIBRARY=${MHD_LIBRARY}
               -DCMAKE_CXX_FLAGS=${JSONRPC_CXX_FLAGS})

ExternalProject_Add(jsonrpc-project
    PREFIX src/depends/jsonrpc
    URL https://github.com/Zilliqa/libjson-rpc-cpp/archive/v1.3.0-time-patch.tar.gz
    URL_HASH SHA256=cfa2051f24deeba73b92b8f2f7c198eeafb148f7d71e914bfa1a5a33e895f1c8
    # On Windows it tries to install this dir. Create it to prevent failure.
    PATCH_COMMAND cp ${CMAKE_SOURCE_DIR}/vcpkg.json <SOURCE_DIR> && cmake -E make_directory <SOURCE_DIR>/win32-deps/include
    CMAKE_ARGS ${CMAKE_ARGS}
    # overwrite build and install commands to force Release build on MSVC.
    BUILD_COMMAND cmake --build <BINARY_DIR> --config Release
    INSTALL_COMMAND cmake --build <BINARY_DIR> --config Release --target install
)


# Create jsonrpc imported libraries
if (WIN32)
    # On Windows CMAKE_INSTALL_PREFIX is ignored and installs to dist dir.
    ExternalProject_Get_Property(jsonrpc-project BINARY_DIR)
    set(INSTALL_DIR ${BINARY_DIR}/dist)
else()
    ExternalProject_Get_Property(jsonrpc-project INSTALL_DIR)
endif()

set(JSONRPC_INCLUDE_DIR ${INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${JSONRPC_INCLUDE_DIR})  # Must exist.

add_library(jsonrpc::common STATIC IMPORTED)
set_property(TARGET jsonrpc::common PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jsonrpccpp-common${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET jsonrpc::common PROPERTY INTERFACE_LINK_LIBRARIES ${JSONCPP_LINK_TARGETS} gcov)
set_property(TARGET jsonrpc::common PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${JSONRPC_INCLUDE_DIR} ${JSONCPP_INCLUDE_DIR})
link_libraries(--coverage)
link_libraries(-lgcov)
add_dependencies(jsonrpc::common jsonrpc-project)

add_library(jsonrpc::client STATIC IMPORTED)
set_property(TARGET jsonrpc::client PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jsonrpccpp-client${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET jsonrpc::client PROPERTY INTERFACE_LINK_LIBRARIES jsonrpc::common ${CURL_LIBRARY})
set_property(TARGET jsonrpc::client PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIR})
add_dependencies(jsonrpc::client jsonrpc-project)

add_library(jsonrpc::server STATIC IMPORTED)
set_property(TARGET jsonrpc::server PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jsonrpccpp-server${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET jsonrpc::server PROPERTY INTERFACE_LINK_LIBRARIES jsonrpc::common ${MHD_LIBRARY})
set_property(TARGET jsonrpc::server PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MHD_INCLUDE_DIR})
add_dependencies(jsonrpc::server jsonrpc-project)


unset(INSTALL_DIR)
unset(CMAKE_ARGS)
unset(JSONRPC_CXX_FLAGS)
