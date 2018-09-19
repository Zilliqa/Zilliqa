#!/bin/bash
#
# This script is dedicated for CI use
#

set -e

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

dir=build

# assume that it is run from project root directory
cmake -H. -B${dir} -DCMAKE_BUILD_TYPE=Debug -DTESTS=ON -DENABLE_COVERAGE=ON
cmake --build ${dir} -- -j${n_parallel}
cmake --build ${dir} --target clang-format

# remember to append `|| exit` after the commands added in if-then-else
if [ "$os" = "Linux" ]
then
    cmake --build ${dir} --target clang-tidy 2>/dev/null || exit 1
    # The target Zilliqa_coverage already includes "ctest" command, see cmake/CodeCoverage.cmake
    cmake --build ${dir} --target Zilliqa_coverage || exit 1
    ./scripts/ci_xml_checker.sh constants.xml || exit 1
    ./scripts/ci_xml_checker.sh constants_local.xml || exit 1
else
    cd build && ctest --output-on-failure -j${n_parallel} || exit 1
fi

echo "ccache status"
ccache -s
