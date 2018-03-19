cmake_minimum_required(VERSION 2.8)
project(Zilliqa)
# detect operating system
message(STATUS "We are on a ${CMAKE_SYSTEM_NAME} system")
enable_testing()

find_package(Boost COMPONENTS filesystem system unit_test_framework)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)
link_libraries(${JSONCPP_LIBRARIES})
include_directories(${JSONCPP_INCLUDE_DIRS})
link_libraries(${SNAPPY_LIBRARIES})

###################

message(STATUS "checking for module 'openssl'")

find_path(OPENSSL_INCLUDE_DIR
    NAMES openssl/opensslv.h
    PATHS /usr/local/opt/openssl/include /opt/local/include /usr/local/include /usr/include
    DOC "Path in which openssl headers are located."
)

find_path(OPENSSL_LIBRARY_DIR
    NAMES libssl.a libssl.dylib
    PATHS /usr/local/opt/openssl/lib /usr/lib /usr/local/lib
    DOC "Path in which openssl library is located."
)

if(OPENSSL_INCLUDE_DIR AND OPENSSL_LIBRARY_DIR)
  set(OPENSSL_FOUND TRUE)
endif()

if(OPENSSL_FOUND)
    message(STATUS "Found openssl (include: ${OPENSSL_INCLUDE_DIR}, library: ${OPENSSL_LIBRARY_DIR})")
else()
    message(STATUS "  package 'openssl' not found")
endif()

###################

message(STATUS "checking for module 'jsoncpp'")

# Look for the header file.
find_path(JSONCPP_INCLUDE NAMES json/json.h
                          PATHS $ENV{LEVELDB_ROOT}/include /opt/local/include /usr/local/include /usr/include
                          DOC "Path in which the file json/json.h is located." )

# Look for the library.
find_library(JSONCPP_LIBRARY NAMES libjsoncpp.a libjsoncpp.dylib
                             PATHS /usr/lib /usr/local/lib $ENV{LEVELDB_ROOT}/lib
                             DOC "Path to jsoncpp library." )

if(JSONCPP_INCLUDE AND JSONCPP_LIBRARY)
  set(JSONCPP_FOUND TRUE)
endif(JSONCPP_INCLUDE AND JSONCPP_LIBRARY)

if(JSONCPP_FOUND)
    message(STATUS "Found jsoncpp (include: ${JSONCPP_INCLUDE}, library: ${JSONCPP_LIBRARY})")
    set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE})
    set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
else()
    message(STATUS "  package 'jsoncpp' not found")
endif()

###################

message(STATUS "checking for module 'LevelDB'")

# Look for the header file.
find_path(LevelDB_INCLUDE NAMES leveldb/db.h
                          PATHS $ENV{LEVELDB_ROOT}/include /opt/local/include /usr/local/include /usr/include
                          DOC "Path in which the file leveldb/db.h is located." )

# Look for the library.
find_library(LevelDB_LIBRARY NAMES libleveldb.a libleveldb.dylib
                             PATHS /usr/lib /usr/local/lib $ENV{LEVELDB_ROOT}/lib
                             DOC "Path to leveldb library." )

if(LevelDB_INCLUDE AND LevelDB_LIBRARY)
  set(LEVELDB_FOUND TRUE)
endif(LevelDB_INCLUDE AND LevelDB_LIBRARY)

if(LEVELDB_FOUND)
    message(STATUS "Found LevelDB (include: ${LevelDB_INCLUDE}, library: ${LevelDB_LIBRARY})")
    set(LevelDB_INCLUDES ${LevelDB_INCLUDE})
    set(LevelDB_LIBRARIES ${LevelDB_LIBRARY})
else()
    message(STATUS "  package 'LevelDB' not found")
endif()

# Look for the library.
find_library(SNAPPY_LIBRARY NAMES libsnappy.a
                             PATHS /usr/lib /usr/local/lib $ENV{LEVELDB_ROOT}/lib
                             DOC "Path to snappy library." )

if(SNAPPY_LIBRARY)
    set(SNAPPY_FOUND TRUE)
endif(SNAPPY_LIBRARY)

if(SNAPPY_FOUND)
    message(STATUS "Found SNAPPY (library: ${SNAPPY_LIBRARY})")
    set(SNAPPY_LIBRARIES ${SNAPPY_LIBRARY})
else()
    message(STATUS "  package 'SNAPPY' not found")
endif()

# export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DSTAT_TEST)

if(IS_LOOKUP_NODE)
    add_definitions(-DIS_LOOKUP_NODE)
endif(IS_LOOKUP_NODE)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -pthread -std=c++14 -Wall -ggdb")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -pthread -ljsoncpp -ljsonrpccpp-common -ljsonrpccpp-server -lboost_system -lboost_filesystem -std=c++14 -Wall")
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} \
        ${GCC_COVERAGE_COMPILE_FLAGS} \
        -pthread \
        -stdlib=libc++ \
        -std=c++14 \
        -Wall \
        -ggdb \
        -I${OPENSSL_INCLUDE_DIR} \
        -I/usr/local/Cellar/boost/include \
        -I/usr/local/include" )
    set(CMAKE_EXE_LINKER_FLAGS
        "-L/usr/local/opt/boost/lib \
        ${CMAKE_EXE_LINKER_FLAGS} \
        ${GCC_COVERAGE_LINK_FLAGS} \
        -pthread ${jsoncpp_LIBRARIES} \
        -ljsonrpccpp-common \
        -ljsonrpccpp-server \
        -lboost_system \
        -lboost_filesystem \
        -std=c++14 \
        -Wall \
        -L${OPENSSL_LIBRARY_DIR}")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    # To-do
endif()



add_subdirectory (src)
add_subdirectory (tests)
