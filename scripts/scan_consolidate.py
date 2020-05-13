#!/bin/python3
# Copyright (C) 2020 Zilliqa
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

"""
HOW TO USE THIS SCRIPT:
 1. Set LOG_PARAMETERS to true in constants.xml
 2. Launch testnet
 3. ./testnet.sh download all state-00001-log.txt
 4. ./scan_consolidate.py <num shards> download_state-00001-log.txt
"""

import os
import sys
import time
import fnmatch
import re
import shutil
import csv
from operator import itemgetter

KEYWORD_FBSTAT = '[FBSTAT]'
KEYWORD_MBPCK = '[MBPCKT]'
KEYWORD_MICON = '[MICON]'
KEYWORD_MITXN = '[MITXN]'
KEYWORD_BEGIN = 'BEGIN'
KEYWORD_DONE = 'DONE'
CSV_FILENAME = 'results.csv'
KEYWORD_MBWAIT = '[MIBLKSWAIT]'
KEYWORD_FBCON = '[FBCON]'

STATE_LOG_FILENAME='state-00001-log.txt'
END_POS_TIMESTAMP = 25

# =============================
# Log line extraction functions
# =============================

def get_time(line):
    # The format is [ 18-12-07T09:47:21.817 ], what need to get is 09:47:21.817
    return line[11:24]

def get_epoch_number(line):
    return int(re.search('\[(\d+)\] ({0}|{1})$'.format(KEYWORD_BEGIN,KEYWORD_DONE), line).group(1))

def convert_time_string(strTime):
    a,b,cd = strTime.split(':')
    c,d = cd.split('.')
    return int(a) * 3600000 + int(b) * 60000 + int(c) * 1000 + int(d)

def get_FBSTAT(line):
    result = re.search('\[(\d+)\] Size=(\d+) Time=(\S+) TPS=(\S+) Gas=(\d+)', line)
    return int(result.group(1)), result.group(2), result.group(3), result.group(4), result.group(5)

def get_MBPCK(line):
    result = re.search('Size:(\d+) Epoch:(\d+) Shard:(\d+) Txns:(\d+)', line)
    return int(result.group(1)), int(result.group(2)), int(result.group(3)), int(result.group(4))

def get_MICON(line, next_line):
    start_time = convert_time_string(get_time(line))
    end_time = convert_time_string(get_time(next_line))
    m = re.findall(r'(\[\d+\])',line[END_POS_TIMESTAMP:])
    if(m == None):
        print(line)
    #print(m)
    blocknum = int(m[0][1:-1])
    shard_id = int(m[1][1:-1])
    return blocknum, shard_id, end_time-start_time

def get_MITXN(line):
    result = re.search('\[(\d+)\]$', line)
    return int(result.group(1))

def get_MBWAIT_FBCON(mydict_tmp, mydict, line):
    if line.find(KEYWORD_BEGIN) != -1:
        epoch_num = get_epoch_number(line)
        start_time = convert_time_string(get_time(line))
        mydict_tmp[epoch_num] = start_time
    elif line.find(KEYWORD_DONE)!= -1:
        epoch_num = get_epoch_number(line)
        end_time = convert_time_string(get_time(line))
        start_time = mydict_tmp[epoch_num]
        del mydict_tmp[epoch_num]
        mydict[epoch_num] = end_time - start_time if epoch_num not in mydict else max(mydict[epoch_num], end_time - start_time)

# ============================
# Log file searching functions
# ============================

def filter(string, substr): 
    return [str for str in string if
             any(sub in str for sub in substr)] 

def get_filenames_for_dir(path, substr):
    all_files = os.listdir(path)
    return filter(all_files,[substr])

def search_lookup():
    lookup_file_names = get_filenames_for_dir(LOG_DIR, 'lookup')
    fileName = lookup_file_names[0]
    file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r+')
    lookup_info = {}

    for line in file:
        if line.find(KEYWORD_FBSTAT) != -1:
            epoch_num, size, time, tps, gas = get_FBSTAT(line)
            if epoch_num not in lookup_info:
                lookup_info[epoch_num] = [size, time, tps, {}, gas]
            else:
                lookup_info[epoch_num] = [size, time, tps, lookup_info[epoch_num][3], gas]
        elif line.find(KEYWORD_MBPCK) != -1:
            mbsize, epoch_num, shard_id, txns = get_MBPCK(line)
            if epoch_num not in lookup_info:
                lookup_info[epoch_num] = [0, 0, 0.0, {shard_id: [txns, mbsize]}, 0]
            else:
                lookup_info[epoch_num][3][shard_id] = [txns, mbsize]

    file.close()
    return lookup_info

def search_ds():
    ds_wait_times = {}
    ds_consensus_times = {}
    start_time = 0
    all_ds_files = get_filenames_for_dir(LOG_DIR, 'dsguard')
    for fileName in all_ds_files:
        with open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME) as ds_file:
            ds_wait_times_tmp = {}
            ds_consensus_times_tmp = {}
            for line in ds_file:
                if line.find(KEYWORD_MBWAIT) != -1:
                    get_MBWAIT_FBCON(ds_wait_times_tmp, ds_wait_times, line)
                elif line.find(KEYWORD_FBCON) != -1:
                    get_MBWAIT_FBCON(ds_consensus_times_tmp, ds_consensus_times, line)

    return ds_consensus_times, ds_wait_times

def search_normal():
    all_normal_files = get_filenames_for_dir(LOG_DIR,'normal')
    mb_time_infos = {}
    for fileName in all_normal_files:
        file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r')
        shard_id = 0
        for line in file:
            if line.find(KEYWORD_MICON) != -1:
                bNum, shard_id, mb_time = get_MICON(line, next(file))
                if bNum not in mb_time_infos:
                    mb_time_infos[bNum] = {}
                mb_time_infos[bNum][shard_id] = [mb_time]
            elif line.find(KEYWORD_MITXN) != -1:
                tx_num = get_MITXN(line[END_POS_TIMESTAMP:])
                mb_time_infos[bNum][shard_id].append(tx_num)
        file.close()
    return mb_time_infos

# ===========================
# Report generation functions
# ===========================

def make_csv_header():
    with open(CSV_FILENAME, 'w',newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow((['Epoch #', 'Shard Details', '', '', '', 'DS Wait (MBs) (ms)','Block Time (ms)','Gas Used','TPS']))
        w.writerow((['', 'ID', 'Cons (ms)', '# Txns', 'MB Size (bytes)', '','','','']))

def save_to_csv(list_of_items):
    with open(CSV_FILENAME, 'a', newline='\n') as csvfile:
        for item in list_of_items:
            w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
            w.writerow(item)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('USAGE: ' + sys.argv[0] + '<num shards> <path to state logs folder>')
    else:
        LOG_DIR = sys.argv[2]
        num_shards = int(sys.argv[1])
        # FORMAT: mb_time_infos[block number] = {shard id: [time, tx num]}
        mb_time_infos = search_normal()
        ds_consensus_times, ds_wait_times = search_ds()
        lookup_info = search_lookup()
        final_list = []
        num_epochs = min(len(mb_time_infos), len(ds_consensus_times), len(ds_wait_times), len(lookup_info))
        for epoch_num in range(1, num_epochs + 1):
            a = mb_time_infos[epoch_num]
            b = ds_consensus_times[epoch_num]
            c = ds_wait_times[epoch_num]
            d = lookup_info[epoch_num]
            
            # Create first row for this epoch (contains shard 0 MB stats + other stats)
            shard_id = 0
            temp_list = [epoch_num] # Epoch
            temp_list.append(shard_id) # Shard ID
            temp_list.append(a[shard_id][0]) # Consensus Time
            temp_list.append(a[shard_id][1]) # Num Txns
            if shard_id in d[3]:
                temp_list.append(d[3][shard_id][1]) # MB Size
                assert d[3][shard_id][0] == a[shard_id][1] # Num Txns in lookup log == Num Txns in normal log
            else:
                temp_list.append(0)

            temp_list.append(c) # DS Wait Time (MBs)
            temp_list.append(d[1]) # FB Block Time
            temp_list.append(d[4]) # Gas Used
            temp_list.append(d[2]) # TPS
            final_list.append(temp_list)

            # Create other rows for this epoch (contains shard X MB stats)
            if num_shards > 1:
                temp_list = [''] # Epoch
                for shard_id in range(1, num_shards):
                    temp_list.append(shard_id) # Shard ID
                    temp_list.append(a[shard_id][0]) # Consensus Time
                    temp_list.append(a[shard_id][1]) # Num Txns
                    if shard_id in d[3]:
                        temp_list.append(d[3][shard_id][1]) # MB Size
                        assert d[3][shard_id][0] == a[shard_id][1] # Num Txns in lookup log == Num Txns in normal log
                    else:
                        temp_list.append(0)
                temp_list.append('') # DS Wait Time (MBs)
                temp_list.append('') # FB Block Time
                temp_list.append('') # Gas Used
                temp_list.append('') # TPS
                final_list.append(temp_list)

            # Create last row for this epoch (contains DS MB stats)
            temp_list = [''] # Epoch
            temp_list.append(num_shards) # Shard ID (DS)
            temp_list.append(b) # Consensus Time (FB + DS MB)
            if num_shards in d[3]:
                temp_list.append(d[3][num_shards][0]) # Num Txns (DS MB)
                temp_list.append(d[3][num_shards][1]) # MB Size (DS MB)
            else:
                temp_list.append(0)
                temp_list.append(0)
            temp_list.append('') # DS Wait Time (MBs)
            temp_list.append('') # FB Block Time
            temp_list.append('') # Gas Used
            temp_list.append('') # TPS
            final_list.append(temp_list)
        make_csv_header()
        save_to_csv(final_list)