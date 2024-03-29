cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    message(STATUS "Found ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif ()

project(Zilliqa)

# detect operating system
message(STATUS "We are on a ${CMAKE_SYSTEM_NAME} system")

# Workaround: there's a bug related to protobuf 3.9.1 on vcpkg when compiled with
#             x64-osx-dynamic (see https://github.com/microsoft/vcpkg/issues/8421).
#             This is a simple workaround against this issue.
if (APPLE)
    file(CREATE_LINK
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/lib
        SYMBOLIC)
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#
# check dependencies
#
find_package(jsoncpp CONFIG REQUIRED)
find_package(mongocxx CONFIG REQUIRED)
find_package(bsoncxx CONFIG REQUIRED)
find_package(leveldb CONFIG REQUIRED)
find_package(g3log REQUIRED)
find_package(g3sinks REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(protobuf CONFIG REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Boost COMPONENTS system unit_test_framework program_options REQUIRED)

find_package(Schnorr REQUIRED)
find_package(CryptoUtils REQUIRED)

include_directories(${SCHNORR_INCLUDE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

if (COMMIT_ID)
   message(STATUS "Commit ID included")
   add_definitions(-DCOMMIT_ID=${COMMIT_ID})
endif()

if(OPENCL_MINE)
    message(STATUS "OpenCL enabled")
    find_package(OpenCL REQUIRED)
    add_definitions(-DOPENCL_MINE)
endif ()

# VC related test scenario
# For DS Block Consensus
if (VC_TEST_DS_SUSPEND_1)
    message(STATUS "VC 1 test enabled")
    add_definitions(-DVC_TEST_DS_SUSPEND_1)
endif ()

if (VC_TEST_DS_SUSPEND_3)
    message(STATUS "VC 2 test enabled")
    add_definitions(-DVC_TEST_DS_SUSPEND_3)
endif ()

if (GOVVC_TEST_DS_SUSPEND_3)
    message(STATUS "GOVVC 2 test enabled")
    add_definitions(-DGOVVC_TEST_DS_SUSPEND_3)
endif ()

# For Final Block Consensus
if (VC_TEST_FB_SUSPEND_1)
    message(STATUS "VC 3 test enabled")
    add_definitions(-DVC_TEST_FB_SUSPEND_1)
endif ()

if (VC_TEST_FB_SUSPEND_3)
    message(STATUS "VC 4 test enabled")
    add_definitions(-DVC_TEST_FB_SUSPEND_3)
endif ()

# For View change Block Consensus
if (VC_TEST_VC_SUSPEND_1)
    message(STATUS "VC 5 test enabled")
    add_definitions(-DVC_TEST_VC_SUSPEND_1)
endif ()

if (VC_TEST_VC_SUSPEND_3)
    message(STATUS "VC 6 test enabled")
    add_definitions(-DVC_TEST_VC_SUSPEND_3)
endif ()

if (VC_TEST_VC_PRECHECK_1)
    message(STATUS "VC 7 test enabled")
    add_definitions(-DVC_TEST_VC_PRECHECK_1)
endif ()

if (VC_TEST_VC_PRECHECK_2)
    message(STATUS "VC 8 test enabled")
    add_definitions(-DVC_TEST_VC_PRECHECK_2)
endif ()

if (VC_TEST_FB_SUSPEND_RESPONSE)
    message(STATUS "VC 9 test enabled")
    add_definitions(-DVC_TEST_FB_SUSPEND_RESPONSE)
endif ()

# For Merging DSMB into FINALBLOCK
if (DM_TEST_DM_LESSTXN_ONE)
    message(STATUS "DM 1 test enabled")
    add_definitions(-DDM_TEST_DM_LESSTXN_ONE)
endif ()

if (DM_TEST_DM_LESSTXN_ALL)
    message(STATUS "DM 2 test enabled")
    add_definitions(-DDM_TEST_DM_LESSTXN_ALL)
endif ()

if (DM_TEST_DM_LESSMB_ONE)
    message(STATUS "DM 3 test enabled")
    add_definitions(-DDM_TEST_DM_LESSMB_ONE)
endif ()

if (DM_TEST_DM_LESSMB_ALL)
    message(STATUS "DM 4 test enabled")
    add_definitions(-DDM_TEST_DM_LESSMB_ALL)
endif ()

if (DM_TEST_DM_BAD_ANNOUNCE)
    message(STATUS "DM 5 test enabled")
    add_definitions(-DDM_TEST_DM_BAD_ANNOUNCE)
endif ()

if (DM_TEST_DM_BAD_MB_ANNOUNCE)
    message(STATUS "DM 6 test enabled")
    add_definitions(-DDM_TEST_DM_BAD_MB_ANNOUNCE)
endif ()

if (DM_TEST_DM_MORETXN_LEADER)
    message(STATUS "DM 7 test enabled")
    add_definitions(-DDM_TEST_DM_MORETXN_LEADER)
endif ()

if (DM_TEST_DM_MORETXN_HALF)
    message(STATUS "DM 8 test enabled")
    add_definitions(-DDM_TEST_DM_MORETXN_HALF)
endif ()

if (DM_TEST_DM_MOREMB_HALF)
    message(STATUS "DM 9 test enabled")
    add_definitions(-DDM_TEST_DM_MOREMB_HALF)
endif ()

if (SJ_TEST_SJ_TXNBLKS_PROCESS_SLOW)
    message(STATUS "SJ 1 test enabled")
    add_definitions(-DSJ_TEST_SJ_TXNBLKS_PROCESS_SLOW)
endif ()

if (SJ_TEST_SJ_MISSING_MBTXNS)
    message(STATUS "SJ 2 test enabled")
    add_definitions(-DSJ_TEST_SJ_MISSING_MBTXNS)
endif ()

include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src/depends/cryptoutils/include/)
include_directories(${G3LOG_INCLUDE_DIR})

# export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# customize the flags for RELWITHDEBINFO
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ggdb -DNDEBUG")

set(CMAKE_CXX_STANDARD 20)
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
#add_compile_options(-Werror)
add_compile_options(-pedantic)
add_compile_options(-Wextra)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wno-error=deprecated-declarations)
    add_compile_options(-Wno-unused-parameter)
endif ()

if (THREAD_SANITIZER AND ADDRESS_SANITIZER)
    message(FATAL_ERROR "Cannot use ThreadSanitizer (THREAD_SANITIZER=ON) and AddressSanitizer (ADDRESS_SANITIZER=ON) at the same time")
endif ()

if (THREAD_SANITIZER)
    add_compile_options(-fsanitize=thread)
    link_libraries(-fsanitize=thread)
    message(STATUS "THREAD SANITIZER enabled")
endif ()

if (ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
    add_compile_options(-fno-omit-frame-pointer)
    link_libraries(-fsanitize=address)
    message(STATUS "ADDRESS SANITIZER enabled")
endif ()

if (UNDEF_BEHAVIOR_SANITIZER)
    add_compile_options(-fsanitize=undefined)
    link_libraries(-fsanitize=undefined)
    message(STATUS "UNDEFINED BEHAVIOR SANITIZER enabled")
endif ()

if (LIBFUZZER_SANITIZER)
    include(CodeCoverage)
    link_libraries(--coverage)
    link_libraries(-lgcov)
    add_compile_options(-fsanitize=fuzzer,address,undefined)
    add_compile_options(-fno-omit-frame-pointer)
    link_libraries(-fsanitize=fuzzer,address,undefined)
    message(STATUS "Libfuzzer with address and undefined behavior sanitizer enabled")
endif ()

# FIXME: This following
#
#        add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND})
#
#        needs to be fixed. The same chunk of CMake code (as below: 
#        if (ENABLE_COVERAGE AND CMAKE_COMPILER_IS_GNUCXX) ....
#        exists here, in cryptoutils & Schnorr.
#
#        Once fixed, the following if-case for allowing duplciate targets
#        (which isn't supported in Xcode) can be removed.
if (NOT ${CMAKE_GENERATOR} STREQUAL "Xcode")
    set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS 1)
endif ()
if (ENABLE_COVERAGE AND CMAKE_COMPILER_IS_GNUCXX)
    if (NOT TESTS)
        message(FATAL_ERROR "TESTS is not ON")
    endif ()
    include(CodeCoverage)
    add_compile_options(--coverage)
    link_libraries(--coverage)
    add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND})
    # TODO: remove the hardcoded number in -j option
    setup_target_for_coverage(${PROJECT_NAME}_coverage ctest coverage "--output-on-failure;--timeout;100")
endif ()

# using internal jsonrpc variant
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src/depends)

add_subdirectory(src)
add_subdirectory(daemon)

if (TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# installation

set_target_properties(connectivity buildTxBlockHashesToNums genaccounts genkeypair genTxnBodiesFromS3
    getpub getaddr gentxn signmultisig verifymultisig getnetworkhistory
    getrewardhistory isolatedServer rebuildState sendcmd validateDB zilliqa zilliqad asio_multiplier
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(Common Trie NAT
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if (OPENCL_MINE)
    set_target_properties(ethash-cl PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    USE_SOURCE_PERMISSIONS
)

# add clang-format and clang-tidy targets lastly
if (LLVM_EXTRA_TOOLS)
    include(LLVMExtraTools)
else ()
    message(STATUS "LLVM extra tools NOT found (for clang format/tidy)")
endif ()

include(CPack)
