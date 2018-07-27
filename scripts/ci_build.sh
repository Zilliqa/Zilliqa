#!/bin/bash
#
# This script is dedicated for CI use
#

set -e

if [ "$1" = "lookup" ]
then
    CMAKE_EXTRA_OPTIONS="-DIS_LOOKUP_NODE=1"
fi

# set n_parallel to fully utilize the resources
os=$(uname)
case $os in
    'Linux')
        n_parallel=$(nproc)
        ;;
    'Darwin')
        n_parallel=$(sysctl -n hw.ncpu)
        ;;
    *)
        n_parallel=2
        ;;
esac

echo "n_parallel=${n_parallel}"

echo "ccache configuration"
ccache --version
ccache -M 5G
ccache -p

ccache -z
echo "ccache status"
ccache -s

# assume that it is run from project root directory
mkdir build && cd build
cmake ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=Debug -DTESTS=ON -DENABLE_COVERAGE=ON ..
make -j${n_parallel}
make clang-format
if [ "$os" = "Linux" ]
then
    # this target already include "ctest" command, see cmake/CodeCoverage.cmake
    make Zilliqa_coverage
else
    ctest --output-on-failure -j${n_parallel}
fi

echo "ccache status"
ccache -s
