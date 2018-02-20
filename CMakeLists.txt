cmake_minimum_required(VERSION 2.8)
project (nuQoin)
# detect operating system
message(STATUS "We are on a ${CMAKE_SYSTEM_NAME} system")

find_package(Boost COMPONENTS filesystem system unit_test_framework) 
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)
link_libraries(${JSONCPP_LIBRARIES})
include_directories(${JSONCPP_INCLUDE_DIRS})

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

if(IS_LOOKUP_NODE)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -pthread -std=c++14 -Wall -DSTAT_TEST -ggdb -DIS_LOOKUP_NODE" )
        SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -pthread -ljsoncpp -lboost_system -lboost_filesystem -std=c++14 -Wall -DSTAT_TEST -DIS_LOOKUP_NODE" )
    endif()
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -pthread -stdlib=libc++ -std=c++14 -Wall -DSTAT_TEST -DIS_LOOKUP_NODE -ggdb -I/usr/local/Cellar/openssl/1.0.2/include/openssl/ -I/usr/local/Cellar/boost/1.64.0_1/include -I/usr/local/include" )
        SET( CMAKE_EXE_LINKER_FLAGS "-L/usr/local/opt/boost@1.64/lib ${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -pthread ${jsoncpp_LIBRARIES}  -lboost_system -lboost_filesystem -std=c++14 -Wall -DSTAT_TEST -DIS_LOOKUP_NODE -L/usr/local/Cellar/openssl/1.0.2/lib" )
    endif()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        # To-do
    endif()
endif()

if(NOT IS_LOOKUP_NODE)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -pthread -std=c++14 -Wall -DSTAT_TEST -ggdb" )
        SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -pthread -ljsoncpp -lboost_system -lboost_filesystem -std=c++14 -Wall -DSTAT_TEST" )
    endif()
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS} -pthread -stdlib=libc++ -std=c++14 -Wall -DSTAT_TEST -ggdb -I/usr/local/Cellar/openssl/1.0.2/include/openssl/ -I/usr/local/Cellar/boost/1.64.0_1/include -I/usr/local/include" )
        SET( CMAKE_EXE_LINKER_FLAGS "-L/usr/local/opt/boost@1.64/lib ${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -pthread ${jsoncpp_LIBRARIES}  -lboost_system -lboost_filesystem -lleveldb -std=c++14 -Wall -DSTAT_TEST  -L/usr/local/Cellar/openssl/1.0.2/lib" )
    endif()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        # To-do
    endif()
endif()

add_subdirectory (src)
add_subdirectory (tests)
