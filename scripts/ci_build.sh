#!/bin/bash
# Copyright (C) 2019 Zilliqa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
    ./scripts/license_checker.sh || exit 1
    cmake --build ${dir} --target clang-format || exit 1
    cmake --build ${dir} --target clang-tidy || exit 1
    # The target Zilliqa_coverage already includes "ctest" command, see cmake/CodeCoverage.cmake
    cmake --build ${dir} --target Zilliqa_coverage || exit 1
else
    cd build && ctest --output-on-failure -j${n_parallel} || exit 1
fi

echo "ccache status"
ccache -s
