#!/usr/bin/env python
import requests 
import xml.etree.ElementTree as ET
import re
import tarfile
from clint.textui import progress
import os, sys
import shutil

URL='http://zilliqa-historical-data.s3.amazonaws.com'
CHUNK_SIZE = 4096
EXPEC_LEN = 2



def atoi(text):
    return int(text) if text.isdigit() else text
def natural_keys(text):
    return [ atoi(c) for c in re.split('(\d+)',text) ]


def GetFileFromS3(file, test_net):
	response = requests.get(URL+"/"+file, stream=True)
	filename = file.replace(test_net+"/",'')
	with open(filename,'wb') as f:
		total_length = response.headers.get('content-length')
		total_length = int(total_length)
		for chunk in progress.bar(response.iter_content(chunk_size=CHUNK_SIZE), expected_size=(total_length/CHUNK_SIZE) + 1):
			if chunk:
				f.write(chunk)
				f.flush()

	tf = tarfile.open(filename)
	tf.extractall()

def CleanupAndCreateDir(folderName):
	if os.path.exists("./"+folderName):
		shutil.rmtree(folderName)
	os.mkdir(folderName)
	os.chdir(folderName)
	shutil.copyfile('../dsnodes.xml','dsnodes.xml')
	shutil.copyfile('../constants.xml','constants.xml')
	shutil.copyfile('../config.xml','config.xml')


def run(args):

	if(len(args) < EXPEC_LEN):
		print "Error, expected {} argument(s)\n".format(EXPEC_LEN)
		return

	response = requests.get(URL,params={"prefix":args[1]})
	tree = ET.fromstring(response.text)
	startInd = 5
	files = []
	

	CleanupAndCreateDir(args[0])

	for key in tree[startInd:]:
		 fileString = key[0].text
		 files.append(fileString)

	files.sort(key=natural_keys)
	print files

	GetFileFromS3(files[-1], args[1])

if __name__ == "__main__":
   run(sys.argv[1:])

