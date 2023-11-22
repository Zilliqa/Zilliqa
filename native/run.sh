#!/bin/bash
# Copyright (C) 2023 Zilliqa
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
# This script will start an isolated server and run the python API against it
#
set -e
echo 1
(cd rundirs/native-lookup-0 &&  bash start.sh& )
echo 2
(cd rundirs/native-dsguard-0 &&  bash start.sh& )
echo 3
(cd rundirs/native-dsguard-1 &&  bash start.sh& )
echo 4
(cd rundirs/native-dsguard-2 &&  bash start.sh& )
echo 5
(cd rundirs/native-dsguard-3 &&  bash start.sh& )
echo 6
(cd rundirs/native-seedpub-0 &&  bash start.sh& )
echo 7
(cd rundirs/native-normal-0 &&  bash start.sh& )
echo 8
(cd rundirs/native-multiplier-0 &&  bash start.sh& )
