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
import shutil
import time
import datetime
from concurrent.futures import ThreadPoolExecutor
from threading import Thread, Lock

PERSISTENCE_SNAPSHOT_URL='http://zilliqa-incremental.s3.amazonaws.com'
STATEDELTA_DIFF_URL='http://zilliqa-statedelta.s3.amazonaws.com'
CHUNK_SIZE = 4096
EXPEC_LEN = 2
TESTNET_NAME= 'TEST_NET_NAME'
MAX_WORKER_JOBS = 50

Exclude_txnBodies = True
Exclude_microBlocks = True

BASE_PATH = os.path.dirname(os.path.realpath(sys.argv[0]))
STORAGE_PATH = BASE_PATH
mutex = Lock()

def UploadLock():
	response = requests.get(PERSISTENCE_SNAPSHOT_URL+"/"+TESTNET_NAME+"/.lock")
	if response.status_code == 200:
		return True
	return False

def GetEntirePersistenceFromS3():
	CleanupDir(STORAGE_PATH + "/persistence")
	CreateAndChangeDir(STORAGE_PATH)
	if GetAllObjectsFromS3(PERSISTENCE_SNAPSHOT_URL) == 1 :
		exit(1)


def GetStateDeltaFromS3():
	CleanupCreateAndChangeDir(STORAGE_PATH+'/StateDeltaFromS3')
	GetAllObjectsFromS3(STATEDELTA_DIFF_URL)
	ExtractAllGzippedObjects()

def GetAllObjectsFromS3(url):
	MARKER = ''
	list_of_keyurls = []
	# Try get the entire persistence keys.
	# S3 limitation to get only max 1000 keys. so work around using marker.
	while True:
		response = requests.get(url, params={"prefix":TESTNET_NAME, "max-keys":1000, "marker": MARKER})
		tree = ET.fromstring(response.text)
		startInd = 5
		if(tree[startInd:] == []):
			print("Empty response")
			return 1
		print("[" + str(datetime.datetime.now()) + "] Files to be downloaded:")
		for key in tree[startInd:]:
			key_url = key[0].text
			if (not (Exclude_txnBodies and "txBodies" in key_url) and not (Exclude_microBlocks and "microBlocks" in key_url)):
				list_of_keyurls.append(url+"/"+key_url)
				print(key_url)
		
		istruncated=tree[4].text
		if istruncated == 'true':
			nextkey=list_of_keyurls[-1] 
			MARKER=nextkey
			print(istruncated)
		else:
			break

	with ThreadPoolExecutor(max_workers=MAX_WORKER_JOBS) as pool:
		pool.map(GetPersistenceKey,list_of_keyurls)
		pool.shutdown(wait=True)
	print("[" + str(datetime.datetime.now()) + "]"+" All objects from " + url + " completed!")
	return 0

def GetPersistenceKey(key_url):
	response = requests.get(key_url, stream=True)
	filename = key_url.replace(key_url[:key_url.index(TESTNET_NAME+"/")+len(TESTNET_NAME+"/")],"").strip()

	dirname = os.path.dirname(filename).strip()
	if dirname != "":
		mutex.acquire()
		if not os.path.exists(dirname):
			os.makedirs(dirname)
		mutex.release()

	with open(filename,'wb') as f:
		total_length = response.headers.get('content-length')
		total_length = int(total_length)
		for chunk in progress.bar(response.iter_content(chunk_size=CHUNK_SIZE), expected_size=(total_length/CHUNK_SIZE) + 1):
			if chunk:
				f.write(chunk)
				f.flush()
		print("[" + str(datetime.datetime.now()) + "]"+" Downloaded " + filename + " successfully")

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
	while (True):
		try:
			if(UploadLock() == False):
				print("[" + str(datetime.datetime.now()) + "] Started downloading entire persistence")
				GetEntirePersistenceFromS3()
			else:
				continue

			if(UploadLock() == True):
				print("Upload has been triggered. Downloading StateDelta now can cause inconsistent data. will restart again..")
				continue

			print("Started downloading State-Delta")
			GetStateDeltaFromS3()
			break

		except Exception as e:
			print(e)
			print("Error downloading!! Will try again")
			time.sleep(5)
			continue

	print("[" + str(datetime.datetime.now()) + "] Done!")
	exit(0)

if __name__ == "__main__":
	if len(sys.argv) >= 2:
		if os.path.isabs(sys.argv[1]):
			STORAGE_PATH = sys.argv[1]
		else:
			# Get absolute path w.r.t to script 
			STORAGE_PATH = os.path.join(BASE_PATH, sys.argv[1])

		if len(sys.argv) == 3 and sys.argv[2] == "false":
			Exclude_txnBodies = False
			Exclude_microBlocks = False	
				

	run()
