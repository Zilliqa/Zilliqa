cmake_minimum_required(VERSION 3.16.1)

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
    set(JSON_PREFIX,"json")
    set(JSONCPP_LINK_TARGETS "jsoncpp_lib")
else()
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    include_directories(${JSONCPP_INCLUDE_DIRS})
    link_directories(${JSONCPP_LIBRARY_DIRS})
    set(JSONCPP_LINK_TARGETS "jsoncpp")
    set(JSON_PREFIX "jsoncpp")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(NOT TARGET jsonrpc-project)
   include(InstallJsonrpccpp)
endif()

include(InstallG3log)
find_package(g3logger CONFIG REQUIRED)

include(InstallCryptoUtils)

include(InstallMongo)
find_package(mongocxx CONFIG REQUIRED)
 
include(InstallWebsocketpp)
find_package(websocketpp CONFIG REQUIRED)

include(InstallSchnorr)
include_directories(${SCHNORR_INCLUDE_DIR}/include)

find_package(PythonLibs REQUIRED)

string(REPLACE "." ";" PY_VERSION_LIST ${PYTHONLIBS_VERSION_STRING})
list(GET PY_VERSION_LIST 0 PY_VERSION_MAJOR)
list(GET PY_VERSION_LIST 1 PY_VERSION_MINOR)
list(GET PY_VERSION_LIST 2 PY_VERSION_PATCH)

set(PY_VERSION "${PY_VERSION_MAJOR}${PY_VERSION_MINOR}")


find_package(Boost COMPONENTS python-py${PY_VERSION} QUIET)
find_package(Boost COMPONENTS python${PY_VERSION} QUIET)
find_package(Boost COMPONENTS python3-py${PY_VERSION} QUIET)



if(Boost_PYTHON-PY${PY_VERSION}_FOUND)

set(PY_COMP_SUFFIX "python-py${PY_VERSION}")

elseif(Boost_PYTHON${PY_VERSION}_FOUND)

set(PY_COMP_SUFFIX "python${PY_VERSION}")

elseif(Boost_PYTHON3-PY${PY_VERSION}_FOUND)

set(PY_COMP_SUFFIX "python3-py${PY_VERSION}")

else()

message(FATAL_ERROR "Could not find boost python")

endif()

message(STATUS "Python suffix: ${PY_COMP_SUFFIX}")


message(STATUS "Python Version: ${PY_VERSION_MAJOR}.${PY_VERSION_MINOR}")

find_package(Boost COMPONENTS filesystem system unit_test_framework program_options ${PY_COMP_SUFFIX} REQUIRED)

find_program(HOMEBREW NAMES brew PATH /usr/local/bin)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" AND HOMEBREW)
    execute_process(
        #TODO: upgrade to OpenSSL 1.1.1a
        COMMAND ${HOMEBREW} --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    #TODO: upgrade to OpenSSL 1.1.1a
    # include(InstallOpenSSL)
    # set(OPENSSL_USE_STATIC_LIBS TRUE)
    # set(OPENSSL_ROOT_DIR ${CMAKE_BINARY_DIR}/openssl)
endif()

#TODO: upgrade to OpenSSL 1.1.1a
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

if(GOVVC_TEST_DS_SUSPEND_3)
    message(STATUS "GOVVC 2 test enabled")
    add_definitions(-DGOVVC_TEST_DS_SUSPEND_3)
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

if(VC_TEST_FB_SUSPEND_RESPONSE)
    message(STATUS "VC 9 test enabled")
    add_definitions(-DVC_TEST_FB_SUSPEND_RESPONSE)
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

if(DM_TEST_DM_MORETXN_LEADER)
    message(STATUS "DM 7 test enabled")
    add_definitions(-DDM_TEST_DM_MORETXN_LEADER)
endif()

if(DM_TEST_DM_MORETXN_HALF)
    message(STATUS "DM 8 test enabled")
    add_definitions(-DDM_TEST_DM_MORETXN_HALF)
endif()

if(DM_TEST_DM_MOREMB_HALF)
    message(STATUS "DM 9 test enabled")
    add_definitions(-DDM_TEST_DM_MOREMB_HALF)
endif()

if(SJ_TEST_SJ_TXNBLKS_PROCESS_SLOW)
    message(STATUS "SJ 1 test enabled")
    add_definitions(-DSJ_TEST_SJ_TXNBLKS_PROCESS_SLOW)
endif()

if(SJ_TEST_SJ_MISSING_MBTXNS)
    message(STATUS "SJ 2 test enabled")
    add_definitions(-DSJ_TEST_SJ_MISSING_MBTXNS)
endif()

include(FindProtobuf)
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
include_directories(${PROTOBUF_INSTALL_DIR}/include)

include_directories(${CMAKE_SOURCE_DIR}/src/depends/cryptoutils/include/)

include_directories(${G3LOG_INCLUDE_DIR})

include_directories(${WEBSOCKETPP_INCLUDE_DIR})

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
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-system-dev, libboost-filesystem-dev, libboost-test-dev, libssl-dev, libleveldb-dev, libjsoncpp-dev, libsnappy-dev, cmake, libmicrohttpd-dev, libjsonrpccpp-dev, build-essential, pkg-config, libevent-dev, libminiupnpc-dev, libprotobuf-dev, protobuf-compiler, libboost-program-options-dev")
set(CPACK_PACKAGE_CONTACT "maintainers@zilliqa.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Members of maintainers@zilliqa.com")

# compiler and linker options

add_compile_options(-Wall)
add_compile_options(-Werror)
add_compile_options(-pedantic)
add_compile_options(-Wextra)

if (THREAD_SANITIZER AND ADDRESS_SANITIZER)
    message(FATAL_ERROR "Cannot use ThreadSanitizer (THREAD_SANITIZER=ON) and AddressSanitizer (ADDRESS_SANITIZER=ON) at the same time")
endif()

if (THREAD_SANITIZER)
    add_compile_options(-fsanitize=thread)
    link_libraries(-fsanitize=thread)
    message(STATUS "THREAD SANITIZER enabled")
endif()

if (ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
    add_compile_options(-fno-omit-frame-pointer)
    link_libraries(-fsanitize=address)
    message(STATUS "ADDRESS SANITIZER enabled")
endif()

if (UNDEF_BEHAVIOR_SANITIZER)
    add_compile_options(-fsanitize=undefined)
    link_libraries(-fsanitize=undefined)
    message(STATUS "UNDEFINED BEHAVIOR SANITIZER enabled")
endif()

if (LIBFUZZER_SANITIZER)
    include(CodeCoverage)
    link_libraries(--coverage)
    link_libraries(-lgcov)
    add_compile_options(-fsanitize=fuzzer,address,undefined)
    add_compile_options(-fno-omit-frame-pointer)
    link_libraries(-fsanitize=fuzzer,address,undefined)
    message(STATUS "Libfuzzer with address and undefined behavior sanitizer enabled")
endif()

set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS 1)
if (ENABLE_COVERAGE AND CMAKE_COMPILER_IS_GNUCXX)
    if (NOT TESTS)
        message(FATAL_ERROR "TESTS is not ON")
    endif()
    include(CodeCoverage)
    add_compile_options(--coverage)
    link_libraries(--coverage)
    add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND})
    # TODO: remove the hardcoded number in -j option
    setup_target_for_coverage(${PROJECT_NAME}_coverage ctest coverage "-j1;--output-on-failure")
endif()

add_subdirectory (src)
add_subdirectory (daemon)

if(TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# installation

set_target_properties(zilliqa sendcmd genaccounts genkeypair getpub getaddr gentxn signmultisig verifymultisig zilliqad gensigninitialds grepperf getnetworkhistory getrewardhistory validateDB restore genTxnBodiesFromS3 isolatedServer data_migrate
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(Common Trie CryptoUtils NAT
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
