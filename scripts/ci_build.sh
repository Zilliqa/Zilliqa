#!/bin/bash

set -e

# The script is supposed to be run in project root directory

if [ "$1" = "lookup" ]
then
    CMAKE_EXTRA_OPTIONS="-DIS_LOOKUP_NODE=1"
fi

mkdir build && cd build
cmake ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTS=ON ..
make -j$(nproc)
make clang-format

