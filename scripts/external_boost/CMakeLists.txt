cmake_minimum_required(VERSION 3.16.1)
project(ExternalBoost)

include(ExternalProject)
include(ProcessorCount)

option(BUILD_ALL "Build all boost libraries" OFF)
set(INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR} CACHE STRING "Install prefix")

set(Boost_BUILD_TYPE "release")

set(CMAKE_CXX_STANDARD 14)

ProcessorCount(N_PARALLEL)
if(N_PARALLEL EQUAL 0)
    set(N_PARALLEL 2)
endif()
message(STATUS "N_PARALLEL: ${N_PARALLEL}")

find_package(PythonLibs REQUIRED)
message(STATUS "Python Version: ${PYTHONLIBS_VERSION_STRING} ${PYTHON_INCLUDE_DIRS}")

set(Boost_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -I${PYTHON_INCLUDE_DIRS}/")

if(NOT BUILD_ALL)
    set(Boost_FLAGS --with-libraries=program_options,system,filesystem,python,test)
endif()

ExternalProject_Add(Boost
        URL "https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_79_0.tar.bz2"
        URL_HASH SHA256=475d589d51a7f8b3ba2ba4eda022b170e562ca3b760ee922c146b6c65856ef39
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
        UPDATE_COMMAND ./bootstrap.sh --prefix=${INSTALL_PREFIX} ${Boost_FLAGS} --with-python=python3.6
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ./b2 -j${N_PARALLEL} cxxflags=${Boost_CXX_FLAGS} stage ${Boost_BUILD_TYPE}
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ./b2 install
        )

#TODO other deps may be added, like OpenSSL
