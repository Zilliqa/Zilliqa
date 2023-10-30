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

export DEV_TREE_ROOT=`readlink -f $(pwd)/../..`

# This script and following scripts use DEV_TREE_ROOT to find the root of the dev tree
# this is also referenced in the python code to allow moving trees and OSes


echo "DEV_TREE_ROOT: $DEV_TREE_ROOT"

# takes a copy of constants.xml and constants_local.xml and prepares new versions with .native extension

python ./tests/native/prepare_constants.py $DEV_TREE_ROOT/Zilliqa/constants.xml $DEV_TREE_ROOT/Zilliqa/constants.xml.native
python ./tests/native/prepare_constants.py $DEV_TREE_ROOT/Zilliqa/constants_local.xml $DEV_TREE_ROOT/Zilliqa/constants_local.xml.native

./tests/Node/pre_run.sh && ./tests/Node/test_node_lookup.sh && ./tests/Node/test_node_simple.sh
