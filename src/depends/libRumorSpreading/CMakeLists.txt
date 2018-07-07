cmake_minimum_required(VERSION 3.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.h)

add_library(libRumorSpreading ${SOURCES})