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
from pprint import pformat

KEYWORD_FBSTAT = '[FBSTAT]'
KEYWORD_MBPCK = '[MBPCKT]'
KEYWORD_MICON = '[MICON]'
KEYWORD_MITXN = '[MITXN]'
KEYWORD_BEGIN = 'BEGIN'
KEYWORD_DONE = 'DONE'
KEYWORD_MBWAIT = '[MIBLKSWAIT]'
KEYWORD_FBCON = '[FBCON]'
KEYWORD_TXNPKT = '[TXNPKT]'
KEYWORD_FLBLK = '[FLBLKRECV]'
KEYWORD_PUBKEY = '[IDENT]'

END_POS_TIMESTAMP = 25

STATE_LOG_FILENAME='state-00001-log.txt'
CSV_FILENAME = 'results.csv'

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

def get_TXNPKT_lookup(line):
    result = re.search('\[(\d+)\] Shard=(\d+) NumTx=(\d+)', line)
    return int(result.group(1)), int(result.group(2)), int(result.group(3))

def get_PUBKEY(line):
    result = re.search('\[IDENT\] (\S+)', line)
    return result.group(1)

def get_FLBLK(line):
    receipt_time = convert_time_string(get_time(line))
    result = re.search('\[(\d+)\] Shard=(\d+)', line)
    return receipt_time, int(result.group(1)), int(result.group(2))

def get_TXNPKT_normal(line):
    proc_time = convert_time_string(get_time(line))
    result = re.search('\[(\d+)\] PktEpoch=(\d+) Shard=(\d+) Lookup=(\S+)', line)
    return proc_time, int(result.group(1)), int(result.group(2)), int(result.group(3)), result.group(4)

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

def search_lookup_packets(num_shards):
    all_lookup_files = get_filenames_for_dir(LOG_DIR, 'lookup')
    lookup_packets = {}
    lookup_index = 0
    for fileName in all_lookup_files:
        file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r')
        lookup_packets[lookup_index] = {}
        for line in file:
            if line.find(KEYWORD_TXNPKT) != -1:
                epoch_num, shard_id, txns = get_TXNPKT_lookup(line)
                if epoch_num not in lookup_packets[lookup_index]:
                    lookup_packets[lookup_index][epoch_num] = {}
                    for i in range(0, num_shards):
                        lookup_packets[lookup_index][epoch_num][i] = 'none'
                lookup_packets[lookup_index][epoch_num][shard_id] = txns
        file.close()
        lookup_index += 1
    return lookup_packets

def search_lookup_keys():
    all_lookup_files = get_filenames_for_dir(LOG_DIR, 'lookup')
    lookup_ident = {}
    lookup_index = 0
    for fileName in all_lookup_files:
        file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r')
        for line in file:
            if line.find(KEYWORD_PUBKEY) != -1:
                pubkey = get_PUBKEY(line)
                lookup_ident[pubkey] = lookup_index
                break
        file.close()
        lookup_index += 1

    return lookup_ident

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

def search_normal(num_shards, lookup_ident):
    all_normal_files = get_filenames_for_dir(LOG_DIR,'normal')
    mb_time_infos = {}
    fb_receipt_times = {}
    pkt_proc_times = {}

    for fileName in all_normal_files:
        file = open(LOG_DIR+'/'+fileName+'/'+STATE_LOG_FILENAME,'r')
        shard_id = 0
        for line in file:
            if line.find(KEYWORD_MICON) != -1:
                epoch_num, shard_id, mb_time = get_MICON(line, next(file))
                if epoch_num not in mb_time_infos:
                    mb_time_infos[epoch_num] = {}
                mb_time_infos[epoch_num][shard_id] = [mb_time]
            elif line.find(KEYWORD_MITXN) != -1:
                tx_num = get_MITXN(line[END_POS_TIMESTAMP:])
                mb_time_infos[epoch_num][shard_id].append(tx_num)
            elif line.find(KEYWORD_FLBLK) != -1:
                receipt_time, epoch_num, shard_id = get_FLBLK(line)
                if epoch_num not in fb_receipt_times:
                    fb_receipt_times[epoch_num] = {}
                fb_receipt_times[epoch_num][fileName] = receipt_time
            elif line.find(KEYWORD_TXNPKT) != -1:
                proc_time, epoch_num, pkt_epoch, shard_id, pubkey = get_TXNPKT_normal(line)
                lookup_index = lookup_ident[pubkey]
                if pkt_epoch not in pkt_proc_times:
                    pkt_proc_times[pkt_epoch] = {}
                if shard_id not in pkt_proc_times[pkt_epoch]:
                    pkt_proc_times[pkt_epoch][shard_id] = {}
                    for i in range(0, len(lookup_ident)):
                        pkt_proc_times[pkt_epoch][shard_id][i] = {}
                pkt_proc_times[pkt_epoch][shard_id][lookup_index][fileName] = proc_time
        file.close()

    prevfb_to_pktproc_times = {}
    num_epochs = len(mb_time_infos)

    for epoch_num in range(2, num_epochs + 1):
        prevfb_to_pktproc_times[epoch_num] = {}
        for shard_id in range(0, num_shards):
            prevfb_to_pktproc_times[epoch_num][shard_id] = {}
            for lookup_index in range(0, len(lookup_ident)):
                prevfb_to_pktproc_times[epoch_num][shard_id][lookup_index] = 0
                if epoch_num not in pkt_proc_times:
                    continue
                for shard_file, proc_time in pkt_proc_times[epoch_num][shard_id][lookup_index].items():
                    prevfb_to_pktproc_times[epoch_num][shard_id][lookup_index] = \
                        max(prevfb_to_pktproc_times[epoch_num][shard_id][lookup_index], \
                            proc_time - fb_receipt_times[epoch_num-1][shard_file])

    return mb_time_infos, prevfb_to_pktproc_times

# ===========================
# Report generation functions
# ===========================

def make_csv_header():
    with open(CSV_FILENAME, 'w',newline='\n') as csvfile:
        w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
        w.writerow((['Epoch #', 'Shard Details', '', '', '', '', '', '', 'DS Details', '', '', '', '', '', 'Epoch Details', '', '']))
        w.writerow((['', 'ID', 'Lookup', 'Pkt #Txs', 'Prev FB receipt->Pkt proc (ms)', 'Cons (ms)', '# Txns', 'MB Size (bytes)', 'Lookup', 'Pkt #Txns', 'DS Wait (MBs) (ms)', 'Cons (ms)', '# Txns', 'MB Size (bytes)', 'Block Time (ms)', 'Gas Used', 'TPS']))

def save_to_csv(list_of_items):
    with open(CSV_FILENAME, 'a', newline='\n') as csvfile:
        for item in list_of_items:
            w = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
            w.writerow(item)

def add_rows_for_epoch(epoch_num, lookup_packets, a, b, c, d, e, num_shards):
    CSV_POS_EPOCH = 0
    CSV_POS_DSWAITMBS = 10
    CSV_POS_BLOCKTIME = 14
    CSV_POS_GASUSED = 15
    CSV_POS_TPS = 16

    CSV_POS_SHARD_SHARDID = 1
    CSV_POS_SHARD_LOOKUPID = 2
    CSV_POS_SHARD_PKTNUMTXS = 3
    CSV_POS_SHARD_PREVFBREC = 4
    CSV_POS_SHARD_CONS = 5
    CSV_POS_SHARD_NUMTXNS = 6
    CSV_POS_SHARD_MBSIZE = 7

    CSV_POS_DS_LOOKUPID = 8
    CSV_POS_DS_PKTNUMTXS = 9
    CSV_POS_DS_CONS = 11
    CSV_POS_DS_NUMTXNS = 12
    CSV_POS_DS_MBSIZE = 13

    CSV_COL_COUNT = 17

    num_lookups = len(lookup_packets)
    rows = []

    # Prepare all the rows needed for this epoch
    for i in range(0, num_shards*num_lookups):
        rows.append(['' for i in range(CSV_COL_COUNT)])

    # Add the epoch-level stats
    rows[0][CSV_POS_EPOCH] = epoch_num # Epoch
    rows[0][CSV_POS_DSWAITMBS] = c # DS Wait Time (MBs)
    rows[0][CSV_POS_BLOCKTIME] = d[1] # FB Block Time
    rows[0][CSV_POS_GASUSED] = d[4] # Gas Used
    rows[0][CSV_POS_TPS] = d[2] # TPS

    # Add the shard-level stats
    for shard_id in range(0, num_shards):
        rows[shard_id * num_lookups][CSV_POS_SHARD_SHARDID] = shard_id # Shard ID
        # Prev FB receipt->Pkt proc
        for lookup_index in range(0, num_lookups):
            rows[shard_id * num_lookups + lookup_index][CSV_POS_SHARD_LOOKUPID] = lookup_index # Lookup ID
            rows[shard_id * num_lookups + lookup_index][CSV_POS_SHARD_PKTNUMTXS] = \
                lookup_packets[lookup_index][epoch_num][shard_id] if epoch_num in lookup_packets[lookup_index] else 'none' # Lookup Pkt #Txns
            rows[shard_id * num_lookups + lookup_index][CSV_POS_SHARD_PREVFBREC] = e[shard_id][lookup_index] if epoch_num > 1 else 'NA'
        rows[shard_id * num_lookups][CSV_POS_SHARD_CONS] = a[shard_id][0] # Consensus Time
        rows[shard_id * num_lookups][CSV_POS_SHARD_NUMTXNS] = a[shard_id][1] # Num Txns
        if shard_id in d[3]:
            rows[shard_id * num_lookups][CSV_POS_SHARD_MBSIZE] = d[3][shard_id][1] # MB Size
            assert d[3][shard_id][0] == a[shard_id][1] # Num Txns in lookup log == Num Txns in normal log
        else:
            rows[shard_id * num_lookups][CSV_POS_SHARD_MBSIZE] = 0 # MB Size

    # Add the DS-level stats
    for lookup_index in range(0, num_lookups):
        rows[lookup_index][CSV_POS_DS_LOOKUPID] = lookup_index # Lookup ID
        rows[lookup_index][CSV_POS_DS_PKTNUMTXS] = \
            lookup_packets[lookup_index][epoch_num][shard_id] if epoch_num in lookup_packets[lookup_index] else 'none' # Lookup Pkt #Txns
    rows[0][CSV_POS_DS_CONS] = b # Consensus Time (FB + DS MB)
    rows[0][CSV_POS_DS_NUMTXNS] = d[3][num_shards][0] if num_shards in d[3] else 0 # Num Txns (DS MB)
    rows[0][CSV_POS_DS_MBSIZE] = d[3][num_shards][1] if num_shards in d[3] else 0 # MB Size (DS MB)

    return rows

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('USAGE: ' + sys.argv[0] + '<num shards> <path to state logs folder>')
    else:
        LOG_DIR = sys.argv[2]
        num_shards = int(sys.argv[1])
        # FORMAT: mb_time_infos[block number] = {shard id: [time, tx num]}
        lookup_ident = search_lookup_keys()
        mb_time_infos, prevfb_to_pktproc_times = search_normal(num_shards, lookup_ident)
        ds_consensus_times, ds_wait_times = search_ds()
        lookup_info = search_lookup()
        lookup_packets = search_lookup_packets(num_shards + 1)
        final_list = []
        num_epochs = min(len(mb_time_infos), len(ds_consensus_times), len(ds_wait_times), len(lookup_info), len(lookup_packets[0]))
        print('len(mb_time_infos)=' + str(len(mb_time_infos)))
        print('len(ds_consensus_times)=' + str(len(ds_consensus_times)))
        print('len(ds_wait_times)=' + str(len(ds_wait_times)))
        print('len(lookup_info)=' + str(len(lookup_info)))
        print('len(lookup_packets[0])=' + str(len(lookup_packets[0])))
        for epoch_num in range(1, num_epochs + 1):
            print('Processing epoch_num=' + str(epoch_num))
            a = mb_time_infos[epoch_num]
            b = ds_consensus_times[epoch_num]
            c = ds_wait_times[epoch_num] if epoch_num in ds_wait_times else 'none'
            d = lookup_info[epoch_num]
            e = prevfb_to_pktproc_times[epoch_num] if epoch_num > 1 else None
            rows = add_rows_for_epoch(epoch_num, lookup_packets, a, b, c, d, e, num_shards)
            final_list.extend(rows)
        make_csv_header()
        save_to_csv(final_list)