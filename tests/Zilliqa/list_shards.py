#!/usr/bin/env python
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your
# participation in testing Zilliqa. You may view, compile and run the code for
# that purpose and pursuant to the protocols and algorithms that are programmed
# into, and intended by, the code. You may not do anything else with the code
# without express permission from Zilliqa Research Pte. Ltd., including
# modifying or publishing the code (or any part of it), and developing or
# forming another public or private blockchain network. This source code is
# provided 'as is' and no warranties are given as to title or non-infringement,
# merchantability or fitness for purpose and, to the extent permitted by law,
# all liability for your use of the code is disclaimed. Some programs in this
# code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
# are governed by GPLv3.0 are those programs that are located in the folders
# src/depends and tests/depends and which include a reference to GPLv3 in their
# program files.

import os
import sys
import fnmatch
import re

KEYWORD_EPOCH = '[ProcessVCDSBlocksMessage      ][Epoch'
KEYWORD_ENTRY = '[ProcessEntireShardingStructure] [SHARD'
KEYWORD_END = '[ProcessEntireShardingStructure] END'
REGEX_EPOCH = '\[ProcessVCDSBlocksMessage      \]\[Epoch (.*)\]'
REGEX_SHARD_PEER_PUBKEY = '\[SHARD (.*)\] \[PEER (.*)\] Inserting Pubkey to shard : (.*)'
REGEX_SHARD_PEER_IPINFO = '\[SHARD (.*)\] \[PEER (.*)\] Corresponding peer : <(.*):'

def print_usage():
	print ("Copyright (C) Zilliqa\n")
	print ("Display shard nodes for latest epoch shown in zilliqa log\n"
		"=============================================================\n"
	"Usage:\n\tpython " + sys.argv[0] + " [Lookup zilliqa log]\n")

def scan_file(fileName):
	file = open(fileName, "r+")
	state = 0;

	# State definitions:
	# 0 = initial
	# 1 = found tag KEYWORD_EPOCH
	# 2 = found a peer's pub key

	# Actions per state:
	# 0 = look for tag KEYWORD_EPOCH
	# 1 = look for either KEYWORD_ENTRY or KEYWORD_END. if KEYWORD_ENTRY, look for pub key in same line and go to 2. if KEYWORD_END, wrap up and go back to 0
	# 2 = look for IP info and go back to 1

	for line in file:
		if state == 0:
			shardsAndShardPeers = []
			if line.find(KEYWORD_EPOCH) != -1:
				searchObj = re.search(REGEX_EPOCH, line)
				epochNum = searchObj.group(1)
				state = 1
		elif state == 1:
			if line.find(KEYWORD_ENTRY) != -1:
				searchObj = re.search(REGEX_SHARD_PEER_PUBKEY, line)
				shardNum = int(searchObj.group(1))
				peerNum = int(searchObj.group(2))
				#pubKey = searchObj.group(3) For this version, we don't save the pubkey, we only initialize the entry for the IP info
				if len(shardsAndShardPeers) == shardNum:
					shardsAndShardPeers.insert(shardNum, [])
				state = 2
			elif line.find(KEYWORD_END) != -1:
				print "EPOCH " + epochNum
				outputstr = ","
				for shardIndex in range(len(shardsAndShardPeers)):
					outputstr = outputstr + "SHARD " + str(shardIndex) + ","
				print outputstr
				for peerIndex in range(len(shardsAndShardPeers[0])):
					outputstr = "PEER " + str(peerIndex) + ","
					for shardIndex in range(len(shardsAndShardPeers)):
						outputstr = outputstr + shardsAndShardPeers[shardIndex][peerIndex] + ","
					print outputstr
				print ""
				state = 0
		elif state == 2:
			if line.find(KEYWORD_ENTRY) != -1:
				searchObj = re.search(REGEX_SHARD_PEER_IPINFO, line)
				shardNum = int(searchObj.group(1))
				peerNum = int(searchObj.group(2))
				ipInfo = searchObj.group(3)
				shardsAndShardPeers[shardNum].insert(peerNum, ipInfo)
				state = 1
	file.close()

def main():
	numargs = len(sys.argv)
	if (numargs < 2):
		print_usage()
	else:
		zilliqaLogFile = sys.argv[1]
		if os.path.isfile(zilliqaLogFile) != True:
			print ("File " + zilliqaLogFile + " does not exist!")
			print_usage()
			return
		scan_file(zilliqaLogFile)

if __name__ == "__main__":
	main()
