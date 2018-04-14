cmake_minimum_required(VERSION 3.5.1)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(Zilliqa)

# detect operating system
message(STATUS "We are on a ${CMAKE_SYSTEM_NAME} system")

#
# check dependencies
#
find_package(PkgConfig REQUIRED)

find_package(Boost COMPONENTS filesystem system unit_test_framework)

# pkg_check_modules(JSONCPP jsoncpp)
# link_libraries(${JSONCPP_LIBRARIES})
# include_directories(${JSONCPP_INCLUDE_DIRS})
# link_libraries(${SNAPPY_LIBRARIES})

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

# message(STATUS "checking for module 'jsoncpp'")

# # Look for the header file.
# find_path(JSONCPP_INCLUDE NAMES json/json.h
                          # PATHS $ENV{LEVELDB_ROOT}/include /opt/local/include /usr/local/include /usr/include
                          # DOC "Path in which the file json/json.h is located." )

# # Look for the library.
# find_library(JSONCPP_LIBRARY NAMES libjsoncpp.a libjsoncpp.dylib
                             # PATHS /usr/lib /usr/local/lib $ENV{LEVELDB_ROOT}/lib
                             # DOC "Path to jsoncpp library." )

# if(JSONCPP_INCLUDE AND JSONCPP_LIBRARY)
  # set(JSONCPP_FOUND TRUE)
# endif(JSONCPP_INCLUDE AND JSONCPP_LIBRARY)

# if(JSONCPP_FOUND)
    # message(STATUS "Found jsoncpp (include: ${JSONCPP_INCLUDE}, library: ${JSONCPP_LIBRARY})")
    # set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE})
    # set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
# else()
    # message(STATUS "  package 'jsoncpp' not found")
# endif()

###################

# message(STATUS "checking for module 'LevelDB'")

# # Look for the header file.
# find_path(LevelDB_INCLUDE NAMES leveldb/db.h
                          # PATHS $ENV{LEVELDB_ROOT}/include /opt/local/include /usr/local/include /usr/include
                          # DOC "Path in which the file leveldb/db.h is located." )

# # Look for the library.
# find_library(LevelDB_LIBRARY NAMES libleveldb.a libleveldb.dylib
                             # PATHS /usr/lib /usr/local/lib $ENV{LEVELDB_ROOT}/lib
                             # DOC "Path to leveldb library." )

# if(LevelDB_INCLUDE AND LevelDB_LIBRARY)
  # set(LEVELDB_FOUND TRUE)
# endif(LevelDB_INCLUDE AND LevelDB_LIBRARY)

# if(LEVELDB_FOUND)
    # message(STATUS "Found LevelDB (include: ${LevelDB_INCLUDE}, library: ${LevelDB_LIBRARY})")
    # set(LevelDB_INCLUDES ${LevelDB_INCLUDE})
    # set(LevelDB_LIBRARIES ${LevelDB_LIBRARY})
# else()
    # message(STATUS "  package 'LevelDB' not found")
# endif()

# # Look for the library.
# find_library(SNAPPY_LIBRARY NAMES libsnappy.a
                             # PATHS /usr/lib /usr/local/lib $ENV{LEVELDB_ROOT}/lib
                             # DOC "Path to snappy library." )

# if(SNAPPY_LIBRARY)
    # set(SNAPPY_FOUND TRUE)
# endif(SNAPPY_LIBRARY)

# if(SNAPPY_FOUND)
    # message(STATUS "Found SNAPPY (library: ${SNAPPY_LIBRARY})")
    # set(SNAPPY_LIBRARIES ${SNAPPY_LIBRARY})
# else()
    # message(STATUS "  package 'SNAPPY' not found")
# endif()

# export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# customize the flags for RELWITHDEBINFO
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ggdb -DNDEBUG")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compiler and linker options
add_definitions(-DSTAT_TEST)

if(IS_LOOKUP_NODE)
    add_definitions(-DIS_LOOKUP_NODE)
endif(IS_LOOKUP_NODE)

add_compile_options(-Wall)
add_compile_options(-Werror)
# add_compile_options(-Wextra) #TODO: enable it

# if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -pthread -std=c++14 -ggdb")
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -pthread -ljsoncpp -ljsonrpccpp-common -ljsonrpccpp-server -lboost_system -lboost_filesystem -std=c++14")
# endif()
# #XXX Assume clang is used on OSX platform, so -pthread is dropped
# if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # set(CMAKE_CXX_FLAGS
        # "${CMAKE_CXX_FLAGS} \
        # ${GCC_COVERAGE_COMPILE_FLAGS} \
        # -stdlib=libc++ \
        # -std=c++14 \
        # -ggdb \
        # -I${OPENSSL_INCLUDE_DIR} \
        # -I/usr/local/Cellar/boost/include \
        # -I/usr/local/include" )
    # set(CMAKE_EXE_LINKER_FLAGS
        # "-L/usr/local/opt/boost/lib \
        # ${CMAKE_EXE_LINKER_FLAGS} \
        # ${GCC_COVERAGE_LINK_FLAGS} \
        # ${jsoncpp_LIBRARIES} \
        # -ljsonrpccpp-common \
        # -ljsonrpccpp-server \
        # -lboost_system \
        # -lboost_filesystem \
        # -std=c++14 \
        # ")
# endif()

add_subdirectory (src)

if(TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# add clang-format and clang-tidy targets lastly
include(cmake/LLVMExtraTools.cmake)
