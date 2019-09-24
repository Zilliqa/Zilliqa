#!/usr/bin/env python3
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

import os
import argparse
import time
import subprocess
import json
import socket
import tarfile
import xml.etree.cElementTree as xtree

TAG_NUM_FINAL_BLOCK_PER_POW = "NUM_FINAL_BLOCK_PER_POW"
TESTNET_NAME= "TEST_NET_NAME"
BUCKET_NAME='BUCKET_NAME'
AWS_PERSISTENCE_LOCATION= "s3://"+BUCKET_NAME+"/persistence/"+TESTNET_NAME


def recvall(sock):
    BUFF_SIZE = 4096 # 4 KiB
    data = ""
    while True:
        part = str(sock.recv(BUFF_SIZE),'utf-8')
        data += part
        if len(part) < BUFF_SIZE:
            # either 0 or end of data
            break
    return data

def get_response(params, methodName, host, port):
    data = json.dumps(generate_payload(params, methodName))
    recv = send_packet_tcp(data, host, port)
    if not recv:
        return None
    response = json.loads(recv)
    return response

def generate_payload(params, methodName, id = 1):
    if params and not type(params) is list:
        params = [params]
    payload = {"method":methodName,
               "params":params,
               "jsonrpc":"2.0",
               "id" : id
               }
    return payload

def send_packet_tcp(data, host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
        data=data+'\n'
        data=data.encode()
        sock.sendall(data)
        received = recvall(sock)
    except socket.error:
        print("Socket error")
        sock.close()
        return None
    sock.close()
    return received

def GetCurrentTxBlockNum():
    loaded_json = get_response([], 'GetEpochFin', '127.0.0.1', 4301)
    blockNum = -1
    if loaded_json == None:
        return blockNum
    val = loaded_json["result"]
    if (val != None and val != ''):
        blockNum = int(val) - 1 # -1 because we need TxBlockNum (not epochnum)
    return blockNum + 1

def backUp(curr_blockNum):
    with tarfile.open(TESTNET_NAME + ".tar.gz", "w:gz") as tar:
        for root, dir, files in os.walk("persistence"):
            for file in files:
                fullpath = os.path.join(root, file)
                tar.add(fullpath)

    os.system("aws s3 cp " + TESTNET_NAME + ".tar.gz " + AWS_PERSISTENCE_LOCATION + ".tar.gz");
    os.system("aws s3 cp " + TESTNET_NAME + ".tar.gz " + AWS_PERSISTENCE_LOCATION +  "-" + str(curr_blockNum) + ".tar.gz");
    return None

def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser(description='Automatically backup script')
    parser.add_argument('-f','--frequency', help='Polling frequency in seconds (default = 0 or run once)', required=False, default=0)
    args = vars(parser.parse_args())

    frequency = 0
    if 'frequency' in args:
        frequency = int(args['frequency'])

    isBackup = False

    while True:
        if os.path.isfile(os.path.dirname(os.path.abspath(__file__)) + "/constants.xml"):
            break
        print("Waiting for constants.xml generated...")
        time.sleep(frequency)

    tree = xtree.parse(os.path.dirname(os.path.abspath(__file__)) + "/constants.xml")
    root = tree.getroot()

    for elem in tree.iter(tag=TAG_NUM_FINAL_BLOCK_PER_POW):
        num_final_block_per_pow = (int)(elem.text)

    while True:
        curr_blockNum = GetCurrentTxBlockNum()
        print("Current blockNum = ", curr_blockNum)

        if(curr_blockNum % num_final_block_per_pow == 0):
            isBackup = False

        if(curr_blockNum % num_final_block_per_pow == (num_final_block_per_pow * 0.8)) and isBackup == False:
            print("Starting to back-up persistence now...")
            backUp(curr_blockNum)
            print("Backing up persistence successfully.")
            isBackup = True

        time.sleep(frequency)

if __name__ == "__main__":
	main()
