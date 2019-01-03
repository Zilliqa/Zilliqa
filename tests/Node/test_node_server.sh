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
# This script will run 20 DS and 60 nodes. This script should be run on a high performance machine. 

sudo sysctl net.core.somaxconn=102400; 
sudo sysctl net.core.netdev_max_backlog=65536; 
sudo sysctl net.ipv4.tcp_tw_reuse=1; 
sudo sysctl -w net.ipv4.tcp_rmem='65536 873800 1534217728';
sudo sysctl -w net.ipv4.tcp_wmem='65536 873800 1534217728';
sudo sysctl -w net.ipv4.tcp_mem='65536 873800 1534217728';

python tests/Zilliqa/test_zilliqa_local.py stop
python tests/Zilliqa/test_zilliqa_local.py clean
python tests/Zilliqa/test_zilliqa_local.py setup 60 
python tests/Zilliqa/test_zilliqa_local.py start 20

sleep 20


#set primary 
for ds in {1..20}
do
    python tests/Zilliqa/test_zilliqa_local.py sendcmd $ds 01000000000000000000000000000100007F00001389
done
sleep 10

# PoW submission should be multicasted to all DS committee members
for node in {21..80}
do
    python tests/Zilliqa/test_zilliqa_local.py startpow $node 20 0000000000000000 10 03 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf
done

echo "end"
# 00 - pow
# 0000000000000000000000000000000000000000000000000000000000000000 - block 0
# 03 - difficulty 
# 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a - rand1
# e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf - rand2 
# 127.0.0.1 - 0000000000000000000000000100007F
# port -      00001388
