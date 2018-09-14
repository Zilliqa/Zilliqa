#!/bin/bash
#
# This script is dedicated for CI use
#

set -e

# add more options to cmake
CMAKE_EXTRA_OPTIONS=""

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
    make clang-tidy 2>/dev/null || exit 1
    make Zilliqa_coverage || exit 1
else
    ctest --output-on-failure -j${n_parallel} || exit 1
fi

echo "ccache status"
ccache -s
