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

import requests 
import xml.etree.ElementTree as ET
import re
import tarfile
from clint.textui import progress
import os, sys
import subprocess
import shutil
import time
import datetime
from concurrent.futures import ThreadPoolExecutor
from threading import Thread, Lock
import hashlib
from distutils.dir_util import copy_tree
from pprint import pformat
import download_static_DB

PERSISTENCE_SNAPSHOT_NAME='incremental'
STATEDELTA_DIFF_NAME='statedelta'
BUCKET_NAME='BUCKET_NAME'
TESTNET_NAME= 'TEST_NET_NAME'

Exclude_txnBodies = True
Exclude_microBlocks = True
Exclude_minerInfo = True

BASE_PATH = os.path.dirname(os.path.realpath(sys.argv[0]))
STORAGE_PATH = BASE_PATH
mutex = Lock()

def getURL():
	return "http://"+BUCKET_NAME+".s3.amazonaws.com"

def UploadLock():
	response = requests.get(getURL()+"/"+PERSISTENCE_SNAPSHOT_NAME+"/"+TESTNET_NAME+"/.lock")
	if response.status_code == 200:
		return True
	return False

def GetCurrentTxBlkNum():
	response = requests.get(getURL()+"/"+PERSISTENCE_SNAPSHOT_NAME+"/"+TESTNET_NAME+"/.currentTxBlk", stream=True)
	if response.status_code == 200:
		return int(response.text.strip())
	else:
		return -1

def GetEntirePersistenceFromS3():
	CreateAndChangeDir(STORAGE_PATH)
	if GetAllObjectsFromS3(getURL(),PERSISTENCE_SNAPSHOT_NAME) == 1 :
		exit(1)

def GetStateDeltaFromS3():
	CleanupCreateAndChangeDir(STORAGE_PATH+'/StateDeltaFromS3')
	GetAllObjectsFromS3(getURL(), STATEDELTA_DIFF_NAME)
	ExtractAllGzippedObjects()

def RsyncBlockChainData(source,destination):
	bashCommand = "rsync --recursive --inplace "
	bashCommand = bashCommand + source + " "
	bashCommand = bashCommand + destination
	print("Command = " + bashCommand)	
	process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
	output, error = process.communicate()
	print("Copied local historical-data persistence to main persistence!")

def GetAllObjectsFromS3(url, folderName=""):
	excludes = ["diff_persistence"]
	if Exclude_txnBodies:
		excludes.append("txEpochs")
		excludes.append("txBodies")
	if Exclude_microBlocks:
		excludes.append("microBlock")
	if Exclude_minerInfo:
		excludes.append("minerInfo")
	exclude_args = []
	for exclude in excludes:
		exclude_args.extend(["--exclude", f"*{exclude}*"])

	attempts = 3
	download_path = f"s3://{BUCKET_NAME}/{folderName}/{TESTNET_NAME}"
	# We can usually expect a single failure during a large sync, because a file will disappear between the start and
	# end of the invocation. There's no harm in retrying a few times, since we'll only download the delta between the
	# each attempt.
	for i in range(attempts):
		print(f"Downloading {download_path} (attempt {i}/{attempts})")
		try:
			command = ["aws", "s3", "sync", "--no-sign-request", "--delete"]
			command.extend(exclude_args)
			command.extend([download_path, "."])
			print(" ".join(command))
			subprocess.run(command, check=True)
			break
		except Exception as e:
			print(f"Download failed: {e}")
			pass
	print("[" + str(datetime.datetime.now()) + "]"+" All objects from " + url + " completed!")
	return 0


def CleanupDir(folderName):
	if os.path.exists(folderName):
		shutil.rmtree(folderName)

def CreateAndChangeDir(folderName):
	if not os.path.exists(folderName):
		os.mkdir(folderName)
	os.chdir(folderName)

def CleanupCreateAndChangeDir(folderName):
	CleanupDir(folderName)
	os.mkdir(folderName)
	os.chdir(folderName)

def ExtractAllGzippedObjects():
	files = [f for f in os.listdir('.') if os.path.isfile(f)]
	for f in files:
		if f.endswith('.tar.gz'):
			tf = tarfile.open(f)
			tf.extractall()
			os.remove(f)
		else:
			if os.path.isfile(f):
				os.remove(f)
			else:
				shutil.rmtree(f)
				
def run():
	dir_name = STORAGE_PATH + "/historical-data"
	main_persistence = STORAGE_PATH + "/persistence"
	try:
		if(Exclude_microBlocks == False and Exclude_txnBodies == False):
			if os.path.exists(dir_name) and os.path.isdir(dir_name):
				if not os.listdir(dir_name):
					# download the static db
					print("Downloading static historical data")
					download_static_DB.start(STORAGE_PATH)
				else:
					print("Already have historical blockchain-data!. Skip downloading again!")
			else:
				# download the static db
				print("Downloading static historical data")
				download_static_DB.start(STORAGE_PATH)
	except Exception as e:
		print("Failed to download static historical data! " + str(e))
		pass

	while (True):
		try:
			currTxBlk = -1
			if(UploadLock() == False):
				currTxBlk = GetCurrentTxBlkNum()
				if(currTxBlk < 0): # wait until current txblk is known
					time.sleep(1)
					continue
				print("[" + str(datetime.datetime.now()) + "] Started downloading entire persistence")
				GetEntirePersistenceFromS3()
			else:
				print("Upload lock acquired, sleeping")
				time.sleep(1)
				continue

			print("Started downloading State-Delta")
			GetStateDeltaFromS3()
			newTxBlk = GetCurrentTxBlkNum()
			# Loop until we've caught up
			if (currTxBlk == newTxBlk):
				print(f"Downloaded persistence at {newTxBlk}")
				break
		except Exception as e:
			print(e)
			print("Error downloading!! Will try again")
			time.sleep(5)
			continue
	if os.path.exists(dir_name):
		RsyncBlockChainData(dir_name+"/", main_persistence)

	print("[" + str(datetime.datetime.now()) + "] Done!")

	return True

def start():
	global Exclude_txnBodies
	global Exclude_microBlocks
	global Exclude_minerInfo
	global STORAGE_PATH
	if len(sys.argv) >= 2:
		if os.path.isabs(sys.argv[1]):
			STORAGE_PATH = sys.argv[1]
		else:
			# Get absolute path w.r.t to script 
			STORAGE_PATH = os.path.join(BASE_PATH, sys.argv[1])

		if len(sys.argv) == 3 and sys.argv[2] == "false":
			Exclude_txnBodies = False
			Exclude_microBlocks = False
			Exclude_minerInfo = False
	return run()

if __name__ == "__main__":
	start()
	
