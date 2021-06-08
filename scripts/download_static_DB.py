#!/usr/bin/env python3
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

import datetime
import hashlib
import os
import re
import requests
import shutil
import sys
import tarfile
import time
import xml.etree.ElementTree as ET
from clint.textui import progress
from concurrent.futures import ThreadPoolExecutor
from pprint import pformat
from threading import Thread, Lock

PERSISTENCE_SNAPSHOT_NAME='blockchain-data'
BUCKET_NAME='BUCKET_NAME'
CHUNK_SIZE = 4096
TESTNET_NAME= 'TEST_NET_NAME'
MAX_WORKER_JOBS = 50
S3_MULTIPART_CHUNK_SIZE_IN_MB = 8
MAX_FAILED_DOWNLOAD_RETRY = 2
BASE_PATH = os.path.dirname(os.path.realpath(sys.argv[0]))
STORAGE_PATH1 = BASE_PATH+'/historical-data'
mutex = Lock()
DOWNLOADED_LIST = []
DOWNLOAD_STARTED_LIST = []
CREATED_FOLDER_LIST = []

def getURL():
	return "http://"+BUCKET_NAME+".s3.amazonaws.com"

def Diff(list1, list2):
	return (list(list(set(list1)-set(list2)) + list(set(list2)-set(list1))))

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

def GetPersistenceKey(key_url):
	global DOWNLOADED_LIST
	global DOWNLOAD_STARTED_LIST
	global CREATED_FOLDER_LIST
	retry_counter = 0
	mutex.acquire()
	DOWNLOAD_STARTED_LIST.append(key_url)
	mutex.release()
	while True:
		try:
			response = requests.get(key_url, stream=True)
		except Exception as e:
			print("Exception occurred while downloading " + key_url + ": " + str(e))
			retry_counter+=1
			if retry_counter > 3:
				print("Failed to download " + key_url + " after " + str(retry_counter) + " retries")
				break
			time.sleep(5)
			print("[Retry: " + str(retry_counter) + "] Downloading again " + key_url)
			continue
		if response.status_code != 200:
			print("Error in downloading file " + key_url + " status_code " + str(response.status_code))
			break
		filename = key_url.replace(key_url[:key_url.index(TESTNET_NAME+"/")+len(TESTNET_NAME+"/")],"").strip()

		dirname = os.path.dirname(filename).strip()
		if dirname != "":
			mutex.acquire()
			# Create subfolder only if (1) it does not exist or (2) it exists and we did not create it ourself
			if not os.path.exists(dirname):
				os.makedirs(dirname)
				CREATED_FOLDER_LIST.append(dirname)
			elif dirname not in CREATED_FOLDER_LIST:
				shutil.rmtree(dirname)
				os.makedirs(dirname)
				CREATED_FOLDER_LIST.append(dirname)
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
			retry_counter += 1
			if retry_counter > 3:
				print("Giving up after " + str(retry_counter) + " retries for file: " + filename + " ! Please check with support team.")
				time.sleep(5)
				os._exit(1)
			print("[Retry: " + str(retry_counter) + "] Downloading again " + filename)
		else:
			break

def LaunchParallelUrlFetch(list_of_keyurls):
	with ThreadPoolExecutor(max_workers=MAX_WORKER_JOBS) as pool:
		pool.map(GetPersistenceKey,list_of_keyurls)
		pool.shutdown(wait=True)

def GetAllObjectsFromS3(url, folderName=""):
	MARKER = ''
	global DOWNLOADED_LIST
	global DOWNLOAD_STARTED_LIST
	global CREATED_FOLDER_LIST
	DOWNLOADED_LIST = []
	DOWNLOAD_STARTED_LIST = []
	CREATED_FOLDER_LIST = []
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
			return False
		print("[" + str(datetime.datetime.now()) + "] Files to be downloaded:")
		lastkey = ''
		for key in tree[startInd:]:
			key_url = key[0].text
			if key_url.endswith("/"):
				continue
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
	while(len(failed_list_of_keyurls) > 0 and failed_retry_download_count < MAX_FAILED_DOWNLOAD_RETRY):
		LaunchParallelUrlFetch(failed_list_of_keyurls)
		failed_list_of_keyurls = Diff(list_of_keyurls, DOWNLOAD_STARTED_LIST) + Diff(list_of_keyurls, DOWNLOADED_LIST)
		failed_retry_download_count = failed_retry_download_count + 1

	if(len(failed_list_of_keyurls) > 0):
		print("DIFF after retry, keyurls vs download started = " + pformat(Diff(list_of_keyurls, DOWNLOAD_STARTED_LIST)))
		print("DIFF after retry, keyurls vs downloaded = " + pformat(Diff(list_of_keyurls, DOWNLOADED_LIST)))
		return False

	print("[" + str(datetime.datetime.now()) + "]"+" All objects from " + url + " completed!")
	return True

def DownloadCompressedFile(url):
	local_filename = url.split('/')[-1]
	with requests.get(url, stream=True) as r:
		try:
			r.raise_for_status()
		except requests.exceptions.HTTPError as e:
			# Whoops it wasn't a 200
			print(e)
			return False
		with open(local_filename, 'wb') as f:
			for chunk in r.iter_content(chunk_size=8192): 
				f.write(chunk)
				f.flush()
	return True

def GetBlockchainDataFromS3(compressed):
	os.chdir(STORAGE_PATH1)
	if compressed:
		file_path = getURL() + "/" + PERSISTENCE_SNAPSHOT_NAME + "/" + TESTNET_NAME + "/" + TESTNET_NAME + ".tar.gz"
		file_name = TESTNET_NAME + ".tar.gz"
		print("File: " + file_path)
		if DownloadCompressedFile(file_path):
			tar = tarfile.open(file_name, "r:gz")
			tar.extractall()
			tar.close()
			os.remove(file_name)
			return True
		else:
			return False
	else:
		return GetAllObjectsFromS3(getURL(), PERSISTENCE_SNAPSHOT_NAME)

def run():
	failed_retry_download_count = 0
	while (True):
		try:
			print("[" + str(datetime.datetime.now()) + "] Started downloading static persistence from blockchain-data")
			if (failed_retry_download_count > MAX_FAILED_DOWNLOAD_RETRY) or GetBlockchainDataFromS3(True):
				break
			print("Error downloading!! Will try again")
		except Exception as e:
			print(e)
			print("Error downloading!! Will try again")
			time.sleep(5)
		finally:
			failed_retry_download_count = failed_retry_download_count + 1

	print("[" + str(datetime.datetime.now()) + "] Done!")
	return True

def start(new_STORAGE_PATH1):
	global STORAGE_PATH1
	if new_STORAGE_PATH1:
		if os.path.isabs(new_STORAGE_PATH1):
			STORAGE_PATH1 = new_STORAGE_PATH1 + "/historical-data"
		else:
			# Get absolute path w.r.t to script
			STORAGE_PATH1 = os.path.join(BASE_PATH, new_STORAGE_PATH1) + "/historical-data"
	# Create historical-data folder if it does not exist
	if not os.path.exists(STORAGE_PATH1):
		os.makedirs(STORAGE_PATH1)
		return run()
	
	if os.path.isdir(STORAGE_PATH1):
		if not os.listdir(STORAGE_PATH1): # empty dir
			# download the static db
			return run()
		else:    
			print("Already have historical blockchain-data!. Skip downloading again!")
	else:
		# Not a directory but some file. Delete file & create directory
		os.remove(STORAGE_PATH1)
		os.makedirs(STORAGE_PATH1)
		# download the static db
		return run()

if __name__ == "__main__":
	start(sys.argv[1] if len(sys.argv) >= 2 else '')