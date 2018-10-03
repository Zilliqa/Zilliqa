#!/bin/bash
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your
# participation in testing Zilliqa. You may view, compile and run the code for
# that purpose and pursuant to the protocols and algorithms that are programmed
# into, and intended by, the code. You may not do anything else with the code
# without express permission from Zilliqa Research Pte. Ltd., including
# modifying or publishing the code (or any part of it), and developing or
# forming another public or private blockchain network. This source code is
# provided 'as is' and no warranties are given as to title or non-infringement,
# merchantability or fitness for purpose and, to the extent permitted by law,
# all liability for your use of the code is disclaimed. Some programs in this
# code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
# are governed by GPLv3.0 are those programs that are located in the folders
# src/depends and tests/depends and which include a reference to GPLv3 in their
# program files.
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

CMAKE_EXTRA_OPTIONS=""

if [ "$os" = "Linux" ]
then
    CMAKE_EXTRA_OPTIONS="$CMAKE_EXTRA_OPTIONS -DLLVM_EXTRA_TOOLS=ON"
fi

# assume that it is run from project root directory
cmake -H. -B${dir} ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=Debug -DTESTS=ON -DENABLE_COVERAGE=ON
cmake --build ${dir} -- -j${n_parallel}

# remember to append `|| exit` after the commands added in if-then-else
if [ "$os" = "Linux" ]
then
    ./scripts/ci_xml_checker.sh constants.xml || exit 1
    ./scripts/ci_xml_checker.sh constants_local.xml || exit 1
    ./scripts/copyright_checker.sh || exit 1
    cmake --build ${dir} --target clang-format || exit 1
    cmake --build ${dir} --target clang-tidy 2>/dev/null || exit 1
    # The target Zilliqa_coverage already includes "ctest" command, see cmake/CodeCoverage.cmake
    cmake --build ${dir} --target Zilliqa_coverage || exit 1
else
    cd build && ctest --output-on-failure -j${n_parallel} || exit 1
fi

echo "ccache status"
ccache -s
