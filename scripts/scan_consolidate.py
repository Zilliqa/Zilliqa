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

KEYWORD_FBSIZE = '[FBSIZE]'
KEYWORD_TIME = '[FBTIME]'
KEYWORD_TPS = '[FBTPS]'
KEYWORD_FBGAS = '[FBGAS]'
KEYWORD_MBPCK = '[MBPCKT]'
KEYWORD_MICON = '[MICON]'
KEYWORD_MITXN = '[MITXN]'
KEYWORD_BEGIN = 'BEGIN'
KEYWORD_DONE = 'DONE'
CSV_FILENAME = 'results.csv'
CSV2_FILENAME = 'extra_results.csv'
KEYWORD_MBWAIT = '[MIBLKSWAIT]'
KEYWORD_FBCON = '[FBCON]'

STATE_LOG_FILENAME='state-00001-log.txt'
END_POS_TIMESTAMP = 25

def get_blocknum_size(line):
    m = re.findall(r'[0-9]+', line)
    if (m == None):
        print(line)
    size = m[0]
    blockNumber = m[1]
    return (int(size),int(blockNumber))

def get_keyword_val(line):
    m = re.search(r'\d+(\.\d+)?',line)
    if(m == None):
        print(line)
    return float(m.group(0))

def get_mb_info(line):
    m = re.findall(r'\d+',line)
    if(m == None):
        print(line)
    size = int(m[0])
    epoch = int(m[1])
    shard = int(m[2])
    return (shard, size)

def make_csv_header(numshards):
    with open(CSV_FILENAME, 'w',newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow((['Block Number'] + (['Shard id', 'Consensus Time (ms)', 'Num Txs'] * numshards) + ['FB Consensus time (ms)', 'DS MBLK Wait time (ms)','FB block time','Total Txns']))
    with open(CSV2_FILENAME, 'w',newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow(['Block Number', 'Size', 'Consensus Time (ms)', 'TPS', 'MB info (Shard id, Size)','Gas Consumed'])

def sort_mb_info(mb_infos):
    mb_infos.sort(key= lambda x:x[0])

def sort_mb_time(mb_infos):
    mb_infos.sort(key=lambda x:(x[0],x[1][0]))

def save_to_csv_lookup(dict_of_items):
    for epoch_num in dict_of_items:
        with open(CSV2_FILENAME, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([epoch_num] + dict_of_items[epoch_num])

def search_lookup():
    lookup_file_names = get_filenames_for_dir(LOG_DIR, 'lookup')
    fileName = lookup_file_names[0]
    file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r+')
    currentBlocknum=-1
    size = 0
    time = 0
    tps = 0
    gas_consumed = 0
    mb_infos = []
    lookup_info = {}

    for line in file:
        if line.find(KEYWORD_FBSIZE) != -1:
            if currentBlocknum >= 0:
                sort_mb_info(mb_infos)
                lookup_info[currentBlocknum] = [size,time,tps*1000000,mb_infos,gas_consumed]
                mb_infos = []
            (size, blockNumber) = get_blocknum_size(line[END_POS_TIMESTAMP:])
            currentBlocknum = blockNumber
        elif line.find(KEYWORD_TIME) != -1:
            time = get_keyword_val(line[END_POS_TIMESTAMP:])
            #print('Time: '+str(time))
        elif line.find(KEYWORD_TPS) != -1:
            tps = get_keyword_val(line[END_POS_TIMESTAMP:])
            #print('TPS: '+str(tps*1000000))
        elif line.find(KEYWORD_FBGAS) != -1:
            gas_consumed = get_keyword_val(line[END_POS_TIMESTAMP:])
            #print('GAS: '+str(gas_consumed))
        elif line.find(KEYWORD_MBPCK) != -1:
            mb_infos.append(get_mb_info(line[END_POS_TIMESTAMP:]))

    file.close()
    return lookup_info

def filter(string, substr): 
    return [str for str in string if
             any(sub in str for sub in substr)] 

def get_filenames_for_dir(path, substr):
    all_files = os.listdir(path)
    return filter(all_files,[substr])

def get_time(line):
    # The format is [ 18-12-07T09:47:21.817 ], what need to get is 09:47:21.817
    return line[11:24]

def convert_time_string(strTime):
    a,b,cd = strTime.split(':')
    c,d = cd.split('.')
    return int(a) * 3600000 + int(b) * 60000 + int(c) * 1000 + int(d)

def get_mb_consensus_info(line,next_line):
    start_time = convert_time_string(get_time(line))
    end_time = convert_time_string(get_time(next_line))
    m = re.findall(r'(\[\d+\])',line[END_POS_TIMESTAMP:])
    if(m == None):
        print(line)
    #print(m)
    blocknum = int(m[0][1:-1])
    shard_id = int(m[1][1:-1])
    return blocknum, [shard_id,end_time-start_time]

def get_epoch_number(line):
    return int(re.search('\[(\d+)\] ({0}|{1})$'.format(KEYWORD_BEGIN,KEYWORD_DONE), line).group(1))

def get_epoch_number_and_time(mydict_tmp, mydict, line):
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
                    get_epoch_number_and_time(ds_wait_times_tmp, ds_wait_times, line)
                elif line.find(KEYWORD_FBCON) != -1:
                    get_epoch_number_and_time(ds_consensus_times_tmp, ds_consensus_times, line)

    return ds_consensus_times, ds_wait_times

def search_normal():
    all_normal_files = get_filenames_for_dir(LOG_DIR,'normal')
    mb_time_infos = {}
    for fileName in all_normal_files:
        file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r')
        for line in file:
            if line.find(KEYWORD_MICON) != -1:
                bNum,mb_time = get_mb_consensus_info(line,next(file))
                if bNum not in mb_time_infos.keys():
                    mb_time_infos[bNum] = []
                mb_time_infos[bNum].append(mb_time)
            if line.find(KEYWORD_MITXN) != -1:
                tx_num = get_keyword_val(line[END_POS_TIMESTAMP:])
                mb_time_infos[bNum][-1].append(tx_num)
        file.close()
    return mb_time_infos

def save_to_csv_time(list_of_items):
    with open(CSV_FILENAME, 'a', newline='\n') as csvfile:
        for item in list_of_items:
            w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
            w.writerow(item)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('USAGE: ' + sys.argv[0] + '<num shards> <path to state logs folder>')
    else:
        LOG_DIR = sys.argv[2]
        # FORMAT: mb_time_infos[block number] = [[shard id, time, tx num], [shard id, time, tx num], [shard id, time, tx num]]
        mb_time_infos = search_normal()
        ds_consensus_times, ds_wait_times = search_ds()
        lookup_times = search_lookup()
        final_list = []
        num_epochs = min(len(mb_time_infos), len(ds_consensus_times), len(ds_wait_times), len(lookup_times))
        for epoch_num in range(1, num_epochs + 1):
            a = sorted(mb_time_infos[epoch_num], key=itemgetter(0))
            b = ds_consensus_times[epoch_num]
            c = ds_wait_times[epoch_num]
            d = lookup_times[epoch_num]
            temp_list = [epoch_num]
            for shardinfo in a:
                for j in range(0,3):
                    temp_list.append(shardinfo[j])
            temp_list.append(b)
            temp_list.append(c)
            temp_list.append(d[1]*1000)
            temp_list.append(int(d[4]))
            final_list.append(temp_list)
        make_csv_header(int(sys.argv[1]))
        final_list = sorted(final_list, key=itemgetter(0))
        save_to_csv_time(final_list)
        save_to_csv_lookup(lookup_times)