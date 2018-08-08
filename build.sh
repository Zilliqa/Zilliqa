#!/bin/bash
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your participation in
# testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
# the protocols and algorithms that are programmed into, and intended by, the code. You may
# not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
# including modifying or publishing the code (or any part of it), and developing or forming
# another public or private blockchain network. This source code is provided ‘as is’ and no
# warranties are given as to title or non-infringement, merchantability or fitness for purpose
# and, to the extent permitted by law, all liability for your use of the code is disclaimed.
# Some programs in this code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
# GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
# and which include a reference to GPLv3 in their program files.

dir=build

if [ "$1" = "lookup" ]
then
    CMAKE_EXTRA_OPTIONS="-DIS_LOOKUP_NODE=1"
    dir=build_lookup
    echo build lookup node
elif [ "$1" = "cuda" ]
then
    CMAKE_EXTRA_OPTIONS="-DCUDA_MINE=1"
    echo build normal node with cuda
fi

cmake -H. -B${dir} ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTS=ON -DCMAKE_INSTALL_PREFIX=..
cmake --build ${dir} -- -j4
cmake --build ${dir} --target clang-format-fix
