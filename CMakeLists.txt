cmake_minimum_required(VERSION 3.5.1)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(Zilliqa)

# detect operating system
message(STATUS "We are on a ${CMAKE_SYSTEM_NAME} system")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#
# check dependencies
#
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)
link_directories(${LIBEVENT_LIBRARY_DIRS})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    pkg_check_modules(MINIUPNPC miniupnpc REQUIRED)
    link_directories(${MINIUPNPC_LIBRARY_DIRS})

    find_package(jsoncpp REQUIRED)
    find_package(libjson-rpc-cpp REQUIRED COMPONENTS jsonrpccommon jsonrpcserver)
    set(JSONCPP_LINK_TARGETS "jsoncpp_lib")
    set(JSONRPCCPP_LINK_TARGETS "libjson-rpc-cpp::jsonrpccommon;libjson-rpc-cpp::jsonrpcserver")
else()
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    include_directories(${JSONCPP_INCLUDE_DIRS})
    link_directories(${JSONCPP_LIBRARY_DIRS})
    set(JSONCPP_LINK_TARGETS "jsoncpp")
    set(JSONRPCCPP_LINK_TARGETS "jsonrpccpp-common;jsonrpccpp-server")
endif()


include(InstallG3log)
find_package(g3logger CONFIG REQUIRED)

find_package(libmongocxx QUIET)
if (NOT ${libmongocxx_FOUND})
    include(InstallMongo)
    find_package(libmongocxx REQUIRED)
endif()
include_directories(${LIBMONGOCXX_INCLUDE_DIRS})

find_package(Boost COMPONENTS filesystem system unit_test_framework)

find_program(HOMEBREW NAMES brew PATH /usr/local/bin)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" AND HOMEBREW)
    execute_process(
        COMMAND ${HOMEBREW} --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(LevelDB REQUIRED)

if(OPENCL_MINE AND CUDA_MINE)
    message(FATAL_ERROR "Cannot support OpenCL (OPENCL_MINE=ON) and CUDA (CUDA=ON) at the same time")
endif()

if(OPENCL_MINE)
    message(STATUS "OpenCL enabled")
    find_package(OpenCL REQUIRED)
    add_definitions(-DOPENCL_MINE)
endif()

if(CUDA_MINE)
    message(STATUS "CUDA enabled")
    find_package(CUDA REQUIRED)
    add_definitions(-DCUDA_MINE)
endif()

if(HEARTBEATTEST)
    message(STATUS "Heartbeat test enabled")
    add_definitions(-DHEARTBEAT_TEST)
endif()

if(FALLBACKTEST)
	add_definitions(-DFALLBACK_TEST)
endif()

# VC related test scenario
# For DS Block Consensus
if(VC_TEST_DS_SUSPEND_1)
    message(STATUS "VC 1 test enabled")
	add_definitions(-DVC_TEST_DS_SUSPEND_1)
endif()

if(VC_TEST_DS_SUSPEND_3)
    message(STATUS "VC 2 test enabled")
	add_definitions(-DVC_TEST_DS_SUSPEND_3)
endif()

# For Final Block Consensus
if(VC_TEST_FB_SUSPEND_1)
    message(STATUS "VC 3 test enabled")
	add_definitions(-DVC_TEST_FB_SUSPEND_1)
endif()

if(VC_TEST_FB_SUSPEND_3)
    message(STATUS "VC 4 test enabled")
	add_definitions(-DVC_TEST_FB_SUSPEND_3)
endif()

# For View change Block Consensus
if(VC_TEST_VC_SUSPEND_1)
    message(STATUS "VC 5 test enabled")
	add_definitions(-DVC_TEST_VC_SUSPEND_1)
endif()

if(VC_TEST_VC_SUSPEND_3)
    message(STATUS "VC 6 test enabled")
	add_definitions(-DVC_TEST_VC_SUSPEND_3)
endif()

if(VC_TEST_VC_PRECHECK_1)
    message(STATUS "VC 7 test enabled")
    add_definitions(-DVC_TEST_VC_PRECHECK_1)
endif()

if(VC_TEST_VC_PRECHECK_2)
    message(STATUS "VC 8 test enabled")
    add_definitions(-DVC_TEST_VC_PRECHECK_2)
endif()

# For Merging DSMB into FINALBLOCK
if(DM_TEST_DM_LESSTXN_ONE)
    message(STATUS "DM 1 test enabled")
    add_definitions(-DDM_TEST_DM_LESSTXN_ONE)
endif()

if(DM_TEST_DM_LESSTXN_ALL)
    message(STATUS "DM 2 test enabled")
    add_definitions(-DDM_TEST_DM_LESSTXN_ALL)
endif()

if(DM_TEST_DM_LESSMB_ONE)
    message(STATUS "DM 3 test enabled")
    add_definitions(-DDM_TEST_DM_LESSMB_ONE)
endif()

if(DM_TEST_DM_LESSMB_ALL)
    message(STATUS "DM 4 test enabled")
    add_definitions(-DDM_TEST_DM_LESSMB_ALL)
endif()

if(DM_TEST_DM_BAD_ANNOUNCE)
    message(STATUS "DM 5 test enabled")
    add_definitions(-DDM_TEST_DM_BAD_ANNOUNCE)
endif()

if(DM_TEST_DM_BAD_MB_ANNOUNCE)
    message(STATUS "DM 6 test enabled")
    add_definitions(-DDM_TEST_DM_BAD_MB_ANNOUNCE)
endif()

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR}/src/depends/libethash/include/)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# customize the flags for RELWITHDEBINFO
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ggdb -DNDEBUG")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# pack related variables
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION $ENV{ZIL_VER})
set(CPACK_PACKAGE_NAME $ENV{ZIL_PACK_NAME})
set(CPACK_DEBIAN_PACKAGE_NAME "zilliqa")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-system-dev, libboost-filesystem-dev, libboost-test-dev, libssl-dev, libleveldb-dev, libjsoncpp-dev, libsnappy-dev, cmake, libmicrohttpd-dev, libjsonrpccpp-dev, build-essential, pkg-config, libevent-dev, libminiupnpc-dev, libprotobuf-dev, protobuf-compiler")
set(CPACK_PACKAGE_CONTACT "maintainers@zilliqa.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Members of maintainers@zilliqa.com")

# compiler and linker options

add_compile_options(-Wall)
add_compile_options(-Werror)
add_compile_options(-Wextra)

if (THREAD_SANITIZER AND ADDRESS_SANITIZER)
    message(FATAL_ERROR "Cannot use ThreadSanitizer (THREAD_SANITIZER=ON) and AddressSanitizer (ADDRESS_SANITIZER=ON) at the same time")
endif()

if (THREAD_SANITIZER)
    add_compile_options(-fsanitize=thread)
    link_libraries(-fsanitize=thread)
endif()

if (ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
    add_compile_options(-fno-omit-frame-pointer)
    link_libraries(-fsanitize=address)
endif()

if (ENABLE_COVERAGE AND CMAKE_COMPILER_IS_GNUCXX)
    if (NOT TESTS)
        message(FATAL_ERROR "TESTS is not ON")
    endif()
    include(CodeCoverage)
    add_compile_options(--coverage)
    link_libraries(--coverage)
    add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND})
    # TODO: remove the hardcoded number in -j option
    setup_target_for_coverage(${PROJECT_NAME}_coverage ctest coverage "-j2;--output-on-failure")
endif()

add_subdirectory (src)
add_subdirectory (daemon)

if(TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# installation
set_target_properties(zilliqa sendcmd genkeypair gentxn signmultisig ZilliqaDaemon_AWS gensigninitialds grepperf
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(Common Trie ethash NAT
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(OPENCL_MINE)
    set_target_properties(ethash-cl PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

if(CUDA_MINE)
    set_target_properties(ethash-cuda PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    USE_SOURCE_PERMISSIONS
)

# add clang-format and clang-tidy targets lastly
if(LLVM_EXTRA_TOOLS)
    include(LLVMExtraTools)
endif()

include(CPack)
