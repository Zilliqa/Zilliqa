#!/bin/bash
#
# This script is dedicated for CI use
#

set -e

if [ "$1" = "lookup" ]
then
    CMAKE_EXTRA_OPTIONS="-DIS_LOOKUP_NODE=1"
fi

# assume that it is run from project root directory
mkdir build && cd build
cmake ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTS=ON ..
make -j$(nproc)
make clang-format
ctest --output-on-failure

