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

# assume that it is run from project root directory
mkdir build && cd build
cmake ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTS=ON -DENABLE_COVERAGE=ON ..
make -j${n_parallel}
make clang-format
ctest --output-on-failure
if [ "$os" = "Linux" ]
then
    make -j${n_parallel} Zilliqa_coverage
fi
