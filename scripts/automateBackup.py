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
#
# The script is dedicated for CI use, do not run it on your machine
#

import mmap
import time
import re
import boto3
import tarfile
from boto3.s3.transfer import S3Transfer
from multiprocessing import Process

BUCKET_NAME = 'zilliqa-historical-data'
STATE_LOG_FILENAME = "state-00001-log.txt"
KEYWORD_DSBLK = '[DSBLK]'
SOURCE = './'
TEST_NET_NAME = ""

def UploadToS3(dsEpochNum):
	transfer = boto3.client('s3')
	key = str(dsEpochNum) + '.tar.gz'
	with tarfile.open(key, mode='w:gz') as archive:
		archive.add(SOURCE+'persistence',arcname='persistence')
	transfer.upload_file(key, BUCKET_NAME, TEST_NET_NAME+"/"+key,ExtraArgs={'ACL':'public-read'})
	print("Uploaded")

def get_block_number(line):
	m = re.search(r'\[[0-9 ]+\]', line)
	if (m == None):
		print(line)
	blockNumber = m.group(0)
	blockNumber = blockNumber[1 : len(blockNumber) - 1]
	return int(blockNumber)

def QueryLatestDSEpoch(lastBlockNum):
	while True:
		with open(SOURCE+STATE_LOG_FILENAME,'r') as f:
			m = mmap.mmap(f.fileno(), 0, prot=mmap.PROT_READ)
			i = m.rfind(KEYWORD_DSBLK)
			m.seek(i)
			line = m.readline()
			blockNum = get_block_number(line)
			if(lastBlockNum < blockNum):
				p = Process(target=UploadToS3,args=(blockNum,))
				p.daemon = True
				p.start()
				lastBlockNum = blockNum
		time.sleep(1)

if __name__ == '__main__':
	lastBlockNum = 0
	QueryLatestDSEpoch(lastBlockNum)
