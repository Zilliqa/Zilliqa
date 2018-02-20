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


total_number=10
total_bytes=100
# random_string=''
# # new_char=$(cat /dev/urandom | tr -dc 'A-F0-9' | fold -w 256 | head -n 1 | head --bytes 1)
# new_char_t=AAAAAAAAAA
# new_char_h=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
# new_char_k=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

# for i in $(seq 1 $total_bytes)
# do 
# 	random_string=$random_string$new_char_k
# done
# # echo $random_string

python tests/Zilliqa/test_zilliqa_local.py stop
python tests/Zilliqa/test_zilliqa_local.py clean
python tests/Zilliqa/test_zilliqa_local.py setup $total_number 
python tests/Zilliqa/test_zilliqa_local.py start
sleep 5
python tests/Zilliqa/test_zilliqa_local.py connect
sleep 5

for i in $(seq 1 $total_number)
do 
	python tests/Zilliqa/test_zilliqa_local.py sendcmd $i 00050000000000000000000000000100007F00007530
done

# python tests/Zilliqa/test_zilliqa_local.py sendcmd 1 000400$random_string
python tests/Zilliqa/test_zilliqa_local.py sendcmdrandom 1 $total_bytes
