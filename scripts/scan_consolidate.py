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

import os
import sys
import time
import fnmatch
import re
import shutil
import csv


KEYWORD_FBSIZE='[FBSIZE]'
KEYWORD_TIME='[FBTIME]'
KEYWORD_TPS='[FBTPS]'
KEYWORD_FBGAS='[FBGAS]'
KEYWORD_MBPCK = '[MBPCKT]'
KEYWORD_MICON = '[MICON]'
KEYWORD_MITXN = '[MITXN]'
KEYWORD_BEGIN = 'BEGIN'
KEYWORD_DONE = 'DONE'
CSV_FILENAME = 'results.csv'
CSV2_FILENAME = 'extra_results.csv'
SHARD_SIZE = 3
KEYWORD_MBWAIT = '[MIBLKSWAIT]'
KEYWORD_FBCON = '[FBCON]'


LOG_DIR='test_dir' #Change this to actual direectory

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

def save_to_csv(blockNumber, shard, size, time, tps, gas):
    with open(CSV_FILENAME, 'a', newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',',
                            quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow([blockNumber, shard, time, tps*1000, gas, size])

def make_csv_header():
    with open(CSV_FILENAME, 'w',newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',',
                            quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow(["Block Number", "Shard id", "Consensus Time (ms)", "Num Txs","Shard id", "Consensus Time (ms)", "Num Txs", "Shard id", "Consensus Time (ms)", "Num Txs","FB Consensus time (ms)", "DS MBLK Wait time (ms)","FB block time","Total Txns"])
    with open(CSV2_FILENAME, 'w',newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',',
                            quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow(["Block Number", "Size", "Consensus Time (ms)", "TPS", "MB info (Shard id, Size)","Gas Consumed"])

def sort_mb_info(mb_infos):
    mb_infos.sort(key= lambda x:x[0])
def sort_mb_time(mb_infos):
    mb_infos.sort(key=lambda x:(x[0],x[1][0]))

def save_to_csv_lookup(list_of_items):
    for  i in list_of_items:
        with open(CSV2_FILENAME, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(i)


def search_lookup():

    lookup_file_names = get_filenames_for_dir(LOG_DIR, "lookup")
    fileName = lookup_file_names[0]
    file = open(LOG_DIR+"/"+fileName+"/"+STATE_LOG_FILENAME,'r+')
    currentBlocknum=-1
    size = 0
    time = 0
    tps = 0
    gas_consumed = 0
    mb_infos = []
    lookup_info= []

    for line in file:
        if line.find(KEYWORD_FBSIZE) != -1:
            if currentBlocknum >= 0:
                sort_mb_info(mb_infos)
                lookup_info.append([currentBlocknum,size,time,tps*1000000,mb_infos,gas_consumed])
                mb_infos = []
            (size, blockNumber) = get_blocknum_size(line[END_POS_TIMESTAMP:])
            currentBlocknum = blockNumber
        elif line.find(KEYWORD_TIME) != -1:
            time = get_keyword_val(line[END_POS_TIMESTAMP:])
            #print("Time: "+str(time))
        elif line.find(KEYWORD_TPS) != -1:
            tps = get_keyword_val(line[END_POS_TIMESTAMP:])
            #print("TPS: "+str(tps*1000000))
        elif line.find(KEYWORD_FBGAS) != -1:
            gas_consumed = get_keyword_val(line[END_POS_TIMESTAMP:])
            #print("GAS: "+str(gas_consumed))
        elif line.find(KEYWORD_MBPCK) != -1:
            mb_infos.append(get_mb_info(line[END_POS_TIMESTAMP:]))


    file.close()

    return lookup_info

#####

def filter(string, substr): 
    return [str for str in string if
             any(sub in str for sub in substr)] 

def get_filenames_for_dir(path, substr):
    all_files = os.listdir(path)
    return filter(all_files,[substr])

def get_shard_id(line):
    findResults = re.findall(r'\[[0-9]+\]', line)
    strShard = findResults[1]   
    strReward = strShard[1 : len(strShard) - 1]
    return int(strReward)

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

def get_fb_consensus_info(line, next_line):
    start_time = convert_time_string(get_time(line))
    end_time = convert_time_string(get_time(next_line))

    m = re.findall(r'(\[\d+\])',line[END_POS_TIMESTAMP:])
    if(m == None):
        print(line)
    blocknum = int(m[0][1:-1])

    return [blocknum, end_time-start_time]


def search_ds():

    ds_consensus_times = []
    ds_wait_times = []
    start_time = 0
    epoch_num=1
    all_ds_files = get_filenames_for_dir(LOG_DIR, 'dsguard')
    for fileName in all_ds_files:
        with open(LOG_DIR+"/"+fileName+"/"+STATE_LOG_FILENAME) as ds_file:
            for line in ds_file:
                if line.find(KEYWORD_MBWAIT) != -1:
                    if line.find(KEYWORD_BEGIN) != -1:
                        start_time = convert_time_string(get_time(line))
                        #print(start_time, epoch_num, "begin")
                    elif line.find(KEYWORD_DONE)!= -1:
                         end_time = convert_time_string(get_time(line))
                         #print(end_time, epoch_num, "end")
                         ds_wait_times.append((epoch_num,convert_time_string(get_time(line))-start_time))
                         epoch_num = epoch_num + 1
                         start_time = 0
                if line.find(KEYWORD_FBCON) != -1:
                    ds_consensus_times.append(get_fb_consensus_info(line, next(ds_file)))

    return ds_consensus_times, ds_wait_times


def search_normal():
    all_normal_files = get_filenames_for_dir(LOG_DIR,'normal')

    mb_time_infos = {key: [] for key in range(1,20)}
    mb_shard_txn = []

    for fileName in all_normal_files:
        file = open(LOG_DIR+"/"+fileName+"/"+STATE_LOG_FILENAME,"r")

        for line in file:
            if line.find(KEYWORD_MICON) != -1:
                bNum,mb_time = get_mb_consensus_info(line,next(file))
                #print(bNum, mb_time)
                mb_time_infos[bNum].append(mb_time)
                #print(mb_time_infos)
            if line.find(KEYWORD_MITXN) != -1:
                tx_num = get_keyword_val(line[END_POS_TIMESTAMP:])
                mb_time_infos[bNum][-1].append(tx_num)
        file.close()
    return mb_shard_txn, mb_time_infos

def save_to_csv_time(list_of_items):
    with open(CSV_FILENAME, 'a', newline='\n') as csvfile:
        for item in list_of_items:
            w = csv.writer(csvfile, delimiter=',',
                            quotechar='|', quoting=csv.QUOTE_MINIMAL)
            w.writerow(item)



if __name__ == "__main__":
    make_csv_header()
    mb_shard_txn, mb_time_infos = search_normal()
    ds_consensus_times, ds_wait_times =  search_ds()
    lookup_times = search_lookup()

    mb_time = []

    for (key, value) in mb_time_infos.items():
        mb_time.append([key,value])

    final_list = []
    for (a,b,c,d) in zip(mb_time, ds_consensus_times, ds_wait_times, lookup_times):
        temp_list = []
        temp_list.append(b[0])
        for i in range(0,3):
            for j in range(0,3):
                temp_list.append(a[1][i][j])
        temp_list.append(b[1])
        temp_list.append(c[1])
        temp_list.append(d[2]*1000)
        temp_list.append(int(d[5]))
        final_list.append(temp_list)
    make_csv_header()
    save_to_csv_time(final_list)
    save_to_csv_lookup(lookup_times)

