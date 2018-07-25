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
# 
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
    python tests/Zilliqa/test_zilliqa_local.py startpow $node 20 0000000000000000000000000000000000000000000000000000000000000000 03 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf
done


for port in {21..80}
do
    python tests/Zilliqa/test_zilliqa_local.py sendtxn 50$port
done 

echo "end"
# 00 - pow
# 0000000000000000000000000000000000000000000000000000000000000000 - block 0
# 03 - difficulty 
# 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a - rand1
# e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf - rand2 
# 127.0.0.1 - 0000000000000000000000000100007F
# port -      00001388
