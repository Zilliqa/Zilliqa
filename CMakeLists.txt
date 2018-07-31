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
include(InstallG3log)

find_package(PkgConfig REQUIRED)

find_package(g3logger CONFIG REQUIRED)

find_package(Boost COMPONENTS filesystem system unit_test_framework)

pkg_check_modules(JSONCPP REQUIRED jsoncpp)
include_directories(${JSONCPP_INCLUDE_DIRS})

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

find_package(OpenCL REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# customize the flags for RELWITHDEBINFO
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ggdb -DNDEBUG")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compiler and linker options

if(IS_LOOKUP_NODE)
    add_definitions(-DIS_LOOKUP_NODE)
endif(IS_LOOKUP_NODE)

add_compile_options(-Wall)
add_compile_options(-Werror)
add_compile_options(-Wextra)

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

if(TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# installation
set_target_properties(zilliqa sendcmd sendtxn genkeypair
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(Common Trie ethash NAT ethash-cl
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    USE_SOURCE_PERMISSIONS
)

# add clang-format and clang-tidy targets lastly
include(LLVMExtraTools)
