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



ulimit -n 65535;
ulimit -Sc unlimited; 
ulimit -Hc unlimited;
ulimit -s unlimited; 

python tests/zilliqa/test_zilliqa_local.py start 5

sleep 8
echo "starting..."

#set primary 
for ds in {1..5}
do
    python tests/Zilliqa/test_zilliqa_local.py sendcmd $ds 01000000000000000000000000000100007F00001389
done
sleep 6

# PoW submission should be multicasted to all DS committee members
for node in {6..10}
do
    python tests/Zilliqa/test_zilliqa_local.py startpow $node 5 0000000000000001 05 03 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf
done
