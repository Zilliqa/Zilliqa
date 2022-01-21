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
PRIMARY_BUCKET_NAME= 'PRIMARY_BUCKET_NAME'
SECONDARY_BUCKET_NAME='SECONDARY_BUCKET_NAME'
TESTNET_NAME= 'TEST_NET_NAME'
CHUNK_SIZE = 4096
EXPEC_LEN = 2
MAX_WORKER_JOBS = 50
S3_MULTIPART_CHUNK_SIZE_IN_MB = 8
NUM_DSBLOCK= "PUT_INCRDB_DSNUMS_WITH_STATEDELTAS_HERE"
NUM_FINAL_BLOCK_PER_POW= "PUT_NUM_FINAL_BLOCK_PER_POW_HERE"
MAX_FAILED_FILES_RETRY_DOWNLOAD_ATTEMPTS = 2
MAX_RETRY_DOWNLOAD_INDIVIDUAL_FILE = 2
MAX_RETRY_DOWNLOAD_OVERALL_PERSISTENCE_ATTEMPT = 4
MAX_RETRY_DOWNLOAD_PERSISTENCE_FROM_PRIMARY = 2

Exclude_txnBodies = True
Exclude_microBlocks = True
Exclude_minerInfo = True

BASE_PATH = os.path.dirname(os.path.realpath(sys.argv[0]))
STORAGE_PATH = BASE_PATH
mutex = Lock()

DOWNLOADED_LIST = []
DOWNLOAD_STARTED_LIST = []
CONNECTION_TIMEDOUT_ERRORED_FILE_SET = set()
REQUESTS_CONNECTION_TIMEOUT_IN_SECS = 2

def getURL(bucketName):
	return "http://"+bucketName+".s3.amazonaws.com"

def UploadLock(bucketName):
	response = requests.get(getURL(bucketName)+"/"+PERSISTENCE_SNAPSHOT_NAME+"/"+TESTNET_NAME+"/.lock",timeout=REQUESTS_CONNECTION_TIMEOUT_IN_SECS)
	if response.status_code == 200:
		return True
	return False

def GetCurrentTxBlkNum(bucketName):
	response = requests.get(getURL(bucketName)+"/"+PERSISTENCE_SNAPSHOT_NAME+"/"+TESTNET_NAME+"/.currentTxBlk", stream=True, timeout=REQUESTS_CONNECTION_TIMEOUT_IN_SECS)
	if response.status_code == 200:
		return int(response.text.strip())
	else:
		return -1

def GetEntirePersistenceFromS3(bucketName):
	CleanupDir(STORAGE_PATH + "/persistence")
	CleanupDir(STORAGE_PATH+'/persistenceDiff')
	CreateAndChangeDir(STORAGE_PATH)
	if GetAllObjectsFromS3(getURL(bucketName),PERSISTENCE_SNAPSHOT_NAME) == 1 :
		exit(1)

def GetPersistenceDiffFromS3(bucketName, txnBlkList):
	CleanupCreateAndChangeDir(STORAGE_PATH+'/persistenceDiff')
	for key in txnBlkList:
		filename = "diff_persistence_"+str(key)
		print("Fetching persistence diff for block = " + str(key))
		GetPersistenceKey(getURL(bucketName)+"/"+PERSISTENCE_SNAPSHOT_NAME+"/"+TESTNET_NAME+"/"+filename+".tar.gz")
		if os.path.exists(filename+".tar.gz") :
			ExtractAllGzippedObjects()
			copy_tree(filename, STORAGE_PATH+"/persistence/")
			shutil.rmtree(filename)
	os.chdir(STORAGE_PATH)
	CleanupDir(STORAGE_PATH+'/persistenceDiff')

def GetStateDeltaDiffFromS3(bucketName, txnBlkList):
	if txnBlkList:
		CreateAndChangeDir(STORAGE_PATH+'/StateDeltaFromS3')
		for key in txnBlkList:
			filename = "stateDelta_"+str(key)
			print("Fetching statedelta for block = " + str(key))
			GetPersistenceKey(getURL(bucketName)+"/"+STATEDELTA_DIFF_NAME+"/"+TESTNET_NAME+"/"+filename+".tar.gz")
	else:
		CleanupCreateAndChangeDir(STORAGE_PATH+'/StateDeltaFromS3')
		GetAllObjectsFromS3(getURL(bucketName), STATEDELTA_DIFF_NAME)
	ExtractAllGzippedObjects()
	os.chdir(STORAGE_PATH)

def GetStateDeltaFromS3(bucketName):
	CleanupCreateAndChangeDir(STORAGE_PATH+'/StateDeltaFromS3')
	GetAllObjectsFromS3(getURL(bucketName), STATEDELTA_DIFF_NAME)
	ExtractAllGzippedObjects()

def IsDownloadRestartRequired(currTxBlk, latestTxBlk, NUM_FINAL_BLOCK_PER_POW, NUM_DSBLOCK) :
    print("currTxBlk = "+ str(currTxBlk) + " latestTxBlk = "+ str(latestTxBlk) + " NUM_DSBLOCK = " +str(NUM_DSBLOCK))
    if((latestTxBlk // (NUM_DSBLOCK * NUM_FINAL_BLOCK_PER_POW)) != (currTxBlk // (NUM_DSBLOCK * NUM_FINAL_BLOCK_PER_POW))):
        return True
    return False


def RsyncBlockChainData(source,destination):
	bashCommand = "rsync --recursive --inplace "
	bashCommand = bashCommand + source + " "
	bashCommand = bashCommand + destination
	print("Command = " + bashCommand)	
	process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
	output, error = process.communicate()
	print("Copied local historical-data persistence to main persistence!")

def Diff(list1, list2):
	return (list(list(set(list1)-set(list2)) + list(set(list2)-set(list1))))

def LaunchParallelUrlFetch(list_of_keyurls):
	with ThreadPoolExecutor(max_workers=MAX_WORKER_JOBS) as pool:
		pool.map(GetPersistenceKey,list_of_keyurls)
		pool.shutdown(wait=True)

def RewriteAlternateBucketNameToUrl(failed_url_set):
	replaced_failed_url = []
	modified_url = ""
	for url in failed_url_set:
		modified_url = url
		if modified_url.find(PRIMARY_BUCKET_NAME) > 0:
			modified_url = modified_url.replace(PRIMARY_BUCKET_NAME,SECONDARY_BUCKET_NAME)
			replaced_failed_url.append(modified_url)
		elif modified_url.find(SECONDARY_BUCKET_NAME) > 0:
			modified_url = modified_url.replace(SECONDARY_BUCKET_NAME,PRIMARY_BUCKET_NAME)
			replaced_failed_url.append(modified_url)
	return replaced_failed_url

def GetAllObjectsFromS3(url, folderName=""):
	MARKER = ''
	global DOWNLOADED_LIST
	global DOWNLOAD_STARTED_LIST
	global CONNECTION_TIMEDOUT_ERRORED_FILE_SET
	DOWNLOADED_LIST = []
	DOWNLOAD_STARTED_LIST = []
	list_of_keyurls = []
	failed_list_of_keyurls = []
	prefix = ""
	if folderName:
		prefix = folderName+"/"+TESTNET_NAME
	# Try get the entire persistence keys.
	# S3 limitation to get only max 1000 keys. so work around using marker.
	while True:
		response = requests.get(url, params={"prefix":prefix, "max-keys":1000, "marker": MARKER})
		tree = ET.fromstring(response.text)
		startInd = 5
		if(tree[startInd:] == []):
			print("Empty response")
			return 1
		print("[" + str(datetime.datetime.now()) + "] Files to be downloaded:")
		lastkey = ''
		for key in tree[startInd:]:
			key_url = key[0].text
			if (not (Exclude_txnBodies and "txEpochs" in key_url) and not (Exclude_txnBodies and "txBodies" in key_url) and not (Exclude_microBlocks and "microBlock" in key_url) and not (Exclude_minerInfo and "minerInfo" in key_url) and not ("diff_persistence" in key_url)):
				list_of_keyurls.append(url+"/"+key_url)
				print(key_url)
			lastkey = key_url
		istruncated=tree[4].text
		if istruncated == 'true':
			MARKER=lastkey
			print(istruncated)
		else:
			break

	LaunchParallelUrlFetch(list_of_keyurls)
	DOWNLOADED_LIST.sort()
	DOWNLOAD_STARTED_LIST.sort()
	list_of_keyurls.sort()
	failed_list_of_keyurls = Diff(list_of_keyurls, DOWNLOAD_STARTED_LIST) + Diff(list_of_keyurls, DOWNLOADED_LIST)
	failed_retry_download_count = 0

	print("DIFF keyurls vs download started = " + pformat(Diff(list_of_keyurls, DOWNLOAD_STARTED_LIST)))
	print("DIFF keyurls vs downloaded = " + pformat(Diff(list_of_keyurls, DOWNLOADED_LIST)))

	# retry download missing files
	while(len(failed_list_of_keyurls) > 0 and failed_retry_download_count < MAX_FAILED_FILES_RETRY_DOWNLOAD_ATTEMPTS):
		LaunchParallelUrlFetch(failed_list_of_keyurls)
		failed_list_of_keyurls = Diff(list_of_keyurls, DOWNLOAD_STARTED_LIST) + Diff(list_of_keyurls, DOWNLOADED_LIST)
		failed_retry_download_count = failed_retry_download_count + 1

	if(len(failed_list_of_keyurls) > 0):
		print("DIFF after retry, keyurls vs download started = " + pformat(Diff(list_of_keyurls, DOWNLOAD_STARTED_LIST)))
		print("DIFF after retry, keyurls vs downloaded = " + pformat(Diff(list_of_keyurls, DOWNLOADED_LIST)))

	if(len(CONNECTION_TIMEDOUT_ERRORED_FILE_SET) > 0):
		failed_list_of_keyurls = RewriteAlternateBucketNameToUrl(CONNECTION_TIMEDOUT_ERRORED_FILE_SET)
		if failed_list_of_keyurls:
			LaunchParallelUrlFetch(failed_list_of_keyurls)
		CONNECTION_TIMEDOUT_ERRORED_FILE_SET.clear()

	print("[" + str(datetime.datetime.now()) + "]"+" All objects from " + url + " completed!")
	return 0


def GetPersistenceKey(key_url):
	global DOWNLOADED_LIST
	global DOWNLOAD_STARTED_LIST
	global CONNECTION_TIMEDOUT_ERRORED_FILE_SET
	retry_counter = 0
	mutex.acquire()
	DOWNLOAD_STARTED_LIST.append(key_url)
	mutex.release()
	while True:
		try:
			response = requests.get(key_url, stream=True, timeout=REQUESTS_CONNECTION_TIMEOUT_IN_SECS)
		except requests.exceptions.Timeout as e:
			if retry_counter >= MAX_RETRY_DOWNLOAD_INDIVIDUAL_FILE:
				mutex.acquire()
				CONNECTION_TIMEDOUT_ERRORED_FILE_SET.add(key_url)
				mutex.release()
				break
			time.sleep(5)
			retry_counter+=1
			print("Exception downloading file : " + key_url + ": " +str(e) +" [Retry: " + str(retry_counter) + "] Downloading again ")
			continue
		except Exception as e:
			print("Exception downloading file " + key_url + ": " + str(e))
			if retry_counter >= MAX_RETRY_DOWNLOAD_INDIVIDUAL_FILE:
				print("Failed to download " + key_url + " after " + str(retry_counter) + " retries")
				break
			time.sleep(5)
			retry_counter+=1
			print("[Retry: " + str(retry_counter) + "] Downloading again " + key_url)
			continue
		if response.status_code != 200:
			print("Error in downloading file " + key_url + " status_code " + str(response.status_code))
			break
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
			md5_hash = response.headers.get('ETag')
			for chunk in progress.bar(response.iter_content(chunk_size=CHUNK_SIZE), expected_size=(total_length/CHUNK_SIZE) + 1):
				if chunk:
					f.write(chunk)
					f.flush()
			print("[" + str(datetime.datetime.now()) + "]"+" Downloaded " + filename + " successfully")
			mutex.acquire()
			DOWNLOADED_LIST.append(key_url)
			mutex.release()
		calc_md5_hash = calculate_multipart_etag(filename, S3_MULTIPART_CHUNK_SIZE_IN_MB * 1024 *1024)
		if calc_md5_hash != md5_hash:
			print("md5 checksum mismatch for " + filename + ". Expected: " + md5_hash + ", Actual: " + calc_md5_hash)
			os.remove(filename)
			if retry_counter >= MAX_RETRY_DOWNLOAD_INDIVIDUAL_FILE:
				print("Giving up after " + str(retry_counter) + " retries for file: " + filename + " ! Please check with support team.")
				time.sleep(5)
				os._exit(1)
			retry_counter += 1
			print("[Retry: " + str(retry_counter) + "] Downloading again " + filename)
		else:
			break

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


def calculate_multipart_etag(source_path, chunk_size):

	"""
	calculates a multipart upload etag for amazon s3
	Arguments:
	source_path -- The file to calculate the etage for
	chunk_size -- The chunk size to calculate for.
	"""

	md5s = []
	with open(source_path,'rb') as fp:
		first = True
		while True:
			data = fp.read(chunk_size)
			if not data:
				if first:
					md5s.append(hashlib.md5())
				break
			md5s.append(hashlib.md5(data))
			first = False

	if len(md5s) > 1:
		digests = b"".join(m.digest() for m in md5s)
		new_md5 = hashlib.md5(digests)
		new_etag = '"%s-%s"' % (new_md5.hexdigest(),len(md5s))
	elif len(md5s) == 1: # file smaller than chunk size
		new_etag = '"%s"' % md5s[0].hexdigest()
	else: # empty file
		new_etag = '""'

	return new_etag
				
def run():
	dir_name = STORAGE_PATH + "/historical-data"
	main_persistence = STORAGE_PATH + "/persistence"
	try:
		if(Exclude_microBlocks == False and Exclude_txnBodies == False):
			if os.path.exists(dir_name) and os.path.isdir(dir_name):
				if not os.listdir(dir_name):
					# download the static db
					print("Dowloading static historical-data")
					download_static_DB.start(STORAGE_PATH)
				else:
					print("Already have historical blockchain-data!. Skip downloading again!")
			else:
				# download the static db
				print("Dowloading static historical-data")
				download_static_DB.start(STORAGE_PATH)
	except Exception as e:
		print(e)
		print("Failed to download static historical-data!")
		pass

	bucketName = PRIMARY_BUCKET_NAME
	retry_count = 0
	while (True):
		try:
			currTxBlk = -1
			if(UploadLock(bucketName) == False):
				currTxBlk = GetCurrentTxBlkNum(bucketName)
				if(currTxBlk < 0): # wait until current txblk is known
					time.sleep(1)
					continue
				print("[" + str(datetime.datetime.now()) + "] Started downloading entire persistence")
				GetEntirePersistenceFromS3(bucketName)
			else:
				time.sleep(1)
				continue

			print("Started downloading State-Delta")
			GetStateDeltaFromS3(bucketName)
			#time.sleep(30) // uncomment it for test purpose.
			newTxBlk = GetCurrentTxBlkNum(bucketName)
			if(currTxBlk < newTxBlk):
				# To get new files from S3 if new files where uploaded in meantime
				while(UploadLock(bucketName) == True):
					time.sleep(1)
			else:
				break
			if(IsDownloadRestartRequired(currTxBlk, newTxBlk, NUM_FINAL_BLOCK_PER_POW, NUM_DSBLOCK)):
				print("Redownload persistence as the persistence is overwritten")
				continue
			#get diff of persistence and stadedeltas for newly mined txblocks
			lst = []
			while(currTxBlk < newTxBlk):
				lst.append(currTxBlk+1)
				currTxBlk += 1
			if lst:
				GetPersistenceDiffFromS3(bucketName, lst)
				GetStateDeltaDiffFromS3(bucketName, lst)
			break

		except Exception as e:
			print(e)
			retry_count = retry_count + 1
			if retry_count >= MAX_RETRY_DOWNLOAD_PERSISTENCE_FROM_PRIMARY:
				print("Try persistence download persistence from other bucket")
				bucketName = SECONDARY_BUCKET_NAME
			if retry_count == MAX_RETRY_DOWNLOAD_OVERALL_PERSISTENCE_ATTEMPT:
				print("Error downloading from both of the buckets!! Giving up downloading persistence !!!")
				return False
			time.sleep(5)
			print("Exception downloading persistence : "  +str(e) +" [Retry: " + str(retry_count) + "] Downloading again ")
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
	
