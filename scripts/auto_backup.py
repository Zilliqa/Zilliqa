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
import math
import shutil
import logging
from logging import handlers

TAG_NUM_FINAL_BLOCK_PER_POW = "NUM_FINAL_BLOCK_PER_POW"
TESTNET_NAME= "TEST_NET_NAME"
BUCKET_NAME='BUCKET_NAME'
AWS_PERSISTENCE_LOCATION= "s3://"+BUCKET_NAME+"/persistence/"+TESTNET_NAME

FORMATTER = logging.Formatter(
    "[%(asctime)s %(levelname)-6s %(filename)s:%(lineno)s] %(message)s"
)

rootLogger = logging.getLogger()
rootLogger.setLevel(logging.INFO)

std_handler = logging.StreamHandler()
std_handler.setFormatter(FORMATTER)
rootLogger.addHandler(std_handler)

def setup_logging():
  if not os.path.exists(os.path.dirname(os.path.abspath(__file__)) + "/logs"):
    os.makedirs(os.path.dirname(os.path.abspath(__file__)) + "/logs")
  logfile = os.path.dirname(os.path.abspath(__file__)) + "/logs/auto_backup-log.txt"
  backup_count = 5
  rotating_size = 8
  fh = handlers.RotatingFileHandler(
    logfile, maxBytes=rotating_size * 1024 * 1024, backupCount=backup_count
    )
  fh.setFormatter(FORMATTER)
  rootLogger.addHandler(fh)

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
        logging.warning("Socket error")
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

def CreateTempPersistence():
    bashCommand = "rsync --recursive --inplace --delete -a persistence tempbackup && rsync --recursive --inplace --delete -a persistence tempbackup"
    process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
    output, error = process.communicate()
    logging.info("Copied local persistence to temporary")

def backUp(curr_blockNum):
    CreateTempPersistence()
    os.chdir("tempbackup")
    with tarfile.open(TESTNET_NAME + ".tar.gz", "w:gz") as tar:
        for root, dir, files in os.walk("persistence"):
            for file in files:
                fullpath = os.path.join(root, file)
                tar.add(fullpath)

    os.system("aws s3 cp " + TESTNET_NAME + ".tar.gz " + AWS_PERSISTENCE_LOCATION + ".tar.gz")
    os.system("aws s3 cp " + TESTNET_NAME + ".tar.gz " + AWS_PERSISTENCE_LOCATION +  "-" + str(curr_blockNum) + ".tar.gz")
    os.remove(TESTNET_NAME + ".tar.gz")
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    return None

def main():
    setup_logging()
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser(description='Automatically backup script')
    parser.add_argument('-f','--frequency', help='Polling frequency in seconds (default = 0 or run once)', required=False, default=0)
    args = vars(parser.parse_args())

    # create temp folder
    if os.path.exists('tempbackup'):
        shutil.rmtree('tempbackup')
    os.makedirs('tempbackup')

    frequency = 0
    if 'frequency' in args:
        frequency = int(args['frequency'])

    isBackup = False

    while True:
        if os.path.isfile(os.path.dirname(os.path.abspath(__file__)) + "/constants.xml"):
            break
        logging.info("Waiting for constants.xml generated...")
        time.sleep(frequency)

    tree = xtree.parse(os.path.dirname(os.path.abspath(__file__)) + "/constants.xml")
    root = tree.getroot()

    for elem in tree.iter(tag=TAG_NUM_FINAL_BLOCK_PER_POW):
        num_final_block_per_pow = (int)(elem.text)

    target_backup_final_block = math.floor(num_final_block_per_pow * 0.88)

    while True:
        try:
            curr_blockNum = GetCurrentTxBlockNum()
            print("Current blockNum = %s" % curr_blockNum)

            if(curr_blockNum % num_final_block_per_pow == 0):
                isBackup = False

            if((curr_blockNum % num_final_block_per_pow) == target_backup_final_block) and isBackup == False:
                logging.info("Starting to back-up persistence at blockNum : %s" % curr_blockNum)
                backUp(curr_blockNum)
                logging.info("Backing up persistence successfully.")
                isBackup = True

            time.sleep(frequency)
        except Exception as e:
            logging.warning(e)
            time.sleep(frequency)

if __name__ == "__main__":
	main()
