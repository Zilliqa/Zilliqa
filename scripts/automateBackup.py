#!/usr/bin/env python
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

import mmap
import time
import re
import boto3
import tarfile
from boto3.s3.transfer import S3Transfer
from multiprocessing import Process
import subprocess
import json

BUCKET_NAME = 'zilliqa-historical-data'
SOURCE = './'
TESTNET_NAME= "PUT_TESTNET_HERE"

def UploadToS3(dsEpochNum):
	transfer = boto3.client('s3')
	key = str(dsEpochNum) + '.tar.gz'
	with tarfile.open(key, mode='w:gz') as archive:
		archive.add(SOURCE+'persistence',arcname='persistence')
	transfer.upload_file(key, BUCKET_NAME, TESTNET_NAME +"/"+key,ExtraArgs={'ACL':'public-read'})
	print("Uploaded")

def QueryLatestDSEpoch(lastBlockNum):
	bashCommand = """curl -s --header "Content-Type: application/json" --request POST --data {"method":"GetCurrentDSEpoch","jsonrpc":"2.0","id":1} http://localhost:4201/json-rpc """
	process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
	output, error = process.communicate()
	loaded_json = json.loads(output.decode("utf-8"))
	val = loaded_json["result"]
	if (val != None):
		blockNum = int(val)
	else:
		print("Failed to get the DSBlk")
		return
	if(lastBlockNum < blockNum):
		p = Process(target=UploadToS3,args=(blockNum,))
		p.daemon = True
		p.start()
		lastBlockNum = blockNum
	time.sleep(10)

if __name__ == '__main__':
	lastBlockNum = 0
	QueryLatestDSEpoch(lastBlockNum)
