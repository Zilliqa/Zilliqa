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
import time
import fnmatch
import re

try:
	from sortedcontainers import SortedDict
except ImportError:
	sys.exit("""You need sortedcontainers! Install it by run 'sudo python -m pip install sortedcontainers'""")

STATE_LOG_FILE = 'state-00001-log.txt'
KEYWORD_DSCON = '[DSCON]'
KEYWORD_DSBLK = '[DSBLK]'
KEYWORD_MICON = '[MICON]'
KEYWORD_MIBLK = '[MIBLK]'
KEYWORD_FBCON = '[FBCON]'
KEYWORD_FLBLK = '[FLBLK]'
KEYWORD_BEGIN = 'BGIN'
KEYWORD_DONE = 'DONE'
KEYWORD_SEND = 'SENDING'
KEYWORD_RECV = 'RECVD'
KEYWORD_TXNPROC_START = '[TXNPKTPROC-INITIATE]'
KEYWORD_TXNPROC = '[TXNPKTPROC]'
KEYWORD_IDENT = '[IDENT]'
KEYWORD_DSLEADER = 'DSLD'
KEYWORD_SCLEADER = 'SCLD'
KEYWORD_REWARD = '[REWARD]'
NODE_DROP_OFF_BLOCK_NUMBER = 50 # If node lag behind the latest block large than this number, then it is drop off already

OUTPUT_LINE_FORMAT           = "%s\t%6d\t%s\t%s\t%10d\n"
OUTPUT_LINE_FORMAT_WO_RETURN = "%s\t%6d\t%s\t%s\t%10d\t"

class Consensus:
	blockNumber = 0
	index = 0
	name = ''
	startTime = ''
	endTime = ''
	timeSpan = 0
	shardId = 0

class Node:
	nodeId = 0
	ipAddress = ''

DSConsensusStartTime = SortedDict()
DSConsensusEndTime = SortedDict()
POWCountOfEpoch = SortedDict()

DSBlockSendTime = SortedDict()
DSBlockRecvTime = SortedDict()

DSLeaderIp = SortedDict()
DSLeaderNodeId = SortedDict()

TxnProcStartTime = SortedDict()
TxnProcEndTime = SortedDict()

MIConsensusLeader = SortedDict()
MIConsensusDict = SortedDict()

MIBlockSendTime = SortedDict()
MIBlockRecvTime = SortedDict()

FBConsensusDict = SortedDict()
FLBlockSendTime = SortedDict()
FLBlockRecvTime = SortedDict()

NodeLastBlock = SortedDict()
NodeReward = SortedDict()

mbConsensusStartTime = None
fbConsensusStartTime = None

LatestBlockNumber = 0

def print_usage():
	print ("Copyright (C) Zilliqa\n")
	print ("Profile consensus and communication time from state logs\n"
		"=============================================================\n"
	"Usage:\n\tpython " + sys.argv[0] + " [Log Parent Path] [Output File Path]\n")

def find_files(directory, pattern):
	for root, dirs, files in os.walk(directory):
		for basename in files:
			if fnmatch.fnmatch(basename, pattern):
				filename = os.path.join(root, basename)
				yield filename

def get_time(line):
	# The format is [ 18-12-07T09:47:21.817 ], what need to get is 09:47:21.817
	return line[11:24]

def get_block_number(line):
	m = re.search(r'\[[0-9]+\]', line)
	blockNumber = m.group(0)
	blockNumber = blockNumber[1 : len(blockNumber) - 1]
	return int(blockNumber)

def get_shard_id(line):
	findResults = re.findall(r'\[[0-9]+\]', line)
	strShard = findResults[1]	
	strReward = strShard[1 : len(strShard) - 1]
	return int(strReward)

def check_node_reward(nodeId, line):
	findResults = re.findall(r'\[[0-9]+\]', line)
	strReward = findResults[1]	
	strReward = strReward[1:len(strReward) - 1]
	reward = int(strReward)

	NodeReward[nodeId] = NodeReward[nodeId] + reward

def get_pow_count(line):
	m = re.search(r'POWS = [\d]+', line)
	strPoW = m.group(0)
	powCount = strPoW[len('POWS = ') : len(strPoW)]
	return int(powCount)

def get_ip_address(line):
	m = re.search(r'\[[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+\]', line)
	return m.group(0)

def get_node_id(fileName):
	m = re.search(r'node_[\d]+', fileName)
	nodeId = m.group(0)
	nodeId = nodeId[5 : len(nodeId)]
	return int(nodeId)

def check_micro_consensus(line):
	global mbConsensusStartTime
	blockNumber = get_block_number(line)
	if line.find(KEYWORD_BEGIN) != -1:
		mbConsensusStartTime = get_time(line)
	elif line.find(KEYWORD_DONE) != -1:
		mbConsensus = Consensus()
		mbConsensus.blockNumber = blockNumber
		mbConsensus.shardId = get_shard_id(line)
		mbConsensus.startTime = mbConsensusStartTime
		mbConsensus.endTime = get_time(line)
		mbConsensus.timeSpan = convert_time_string(mbConsensus.endTime) - convert_time_string(mbConsensus.startTime)
		MIConsensusDict.setdefault(blockNumber, SortedDict())[mbConsensus.shardId] = mbConsensus

def check_miblock_broadcast(line):
	blockNumber = get_block_number(line)
	if line.find(KEYWORD_SEND) != -1:
		strSendTime = get_time(line)
		sendTime = convert_time_string(strSendTime)
		# Need to get the earliest one to send out
		if ((not MIBlockSendTime.has_key(blockNumber)) or sendTime < convert_time_string(MIBlockSendTime[blockNumber])):
			MIBlockSendTime[blockNumber] = strSendTime
	elif line.find(KEYWORD_RECV) != -1:
		strRecvTime = get_time(line)
		recvTime = convert_time_string(strRecvTime)
		# Need to get the last one to receive
		if ((not MIBlockRecvTime.has_key(blockNumber)) or recvTime > convert_time_string(MIBlockRecvTime[blockNumber])):
			MIBlockRecvTime[blockNumber] = strRecvTime

def check_final_consensus(line):
	global fbConsensusStartTime
	blockNumber = get_block_number(line)
	if line.find(KEYWORD_BEGIN) != -1:
		fbConsensusStartTime = get_time(line)
	elif line.find(KEYWORD_DONE) != -1:
		fbConsensus = Consensus()
		fbConsensus.blockNumber = blockNumber
		fbConsensus.startTime = fbConsensusStartTime
		fbConsensus.endTime = get_time(line)
		fbConsensus.timeSpan = convert_time_string(fbConsensus.endTime) - convert_time_string(fbConsensus.startTime)
		FBConsensusDict.setdefault(blockNumber, []).append(fbConsensus)

def check_flblk_broadcast(line):
	blockNumber = get_block_number(line)
	if line.find(KEYWORD_SEND) != -1:
		strSendTime = get_time(line)
		sendTime = convert_time_string(strSendTime)
		# Need to get the earliest one to send out
		if ((not FLBlockSendTime.has_key(blockNumber)) or sendTime < convert_time_string(FLBlockSendTime[blockNumber])):
			FLBlockSendTime[blockNumber] = get_time(line)
	elif line.find(KEYWORD_RECV) != -1:
		strRecvTime = get_time(line)
		recvTime = convert_time_string(strRecvTime)
		# Need to get the last one to receive
		if ((not FLBlockRecvTime.has_key(blockNumber)) or recvTime > convert_time_string(FLBlockRecvTime[blockNumber])):
			FLBlockRecvTime[blockNumber] = strRecvTime

def check_leader(line, nodeId):
	if line.find(KEYWORD_DSLEADER) != -1:
		blockNumber = get_block_number(line)
		DSLeaderIp[blockNumber] = get_ip_address(line)
		DSLeaderNodeId[blockNumber] = nodeId
	elif line.find(KEYWORD_SCLEADER) != -1:
		blockNumber = get_block_number(line)
		node = Node()
		node.ipAddress = get_ip_address(line)
		node.nodeId = nodeId
		shardId = get_shard_id(line)
		MIConsensusLeader.setdefault(blockNumber, SortedDict())[shardId] = node

def scan_file(fileName):
	file = open(fileName, "r+")
	mbConsensusStartTime = ''
	fbConsensusStartTime = ''
	blockNumber = 0
	nodeId = get_node_id(fileName)
	NodeReward[nodeId] = 0
	for line in file:
		if line.find(KEYWORD_DSCON) != -1:
			blockNumber = get_block_number(line)
			if line.find(KEYWORD_BEGIN) != -1:
				DSConsensusStartTime[blockNumber] = get_time(line)
				POWCountOfEpoch[blockNumber] = get_pow_count(line)
			elif line.find(KEYWORD_DONE) != -1:
				DSConsensusEndTime[blockNumber] = get_time(line)

		elif line.find(KEYWORD_DSBLK) != -1:
			blockNumber = get_block_number(line)
			if line.find(KEYWORD_SEND) != -1:
				strSendTime = get_time(line)
				sendTime = convert_time_string(strSendTime)
				# Need to get the earliest one to send out
				if ((not DSBlockSendTime.has_key(blockNumber)) or sendTime < convert_time_string(DSBlockSendTime[blockNumber])):
					DSBlockSendTime[blockNumber] = get_time(line)

			elif line.find(KEYWORD_RECV) != -1:
				strRecvTime = get_time(line)
				recvTime = convert_time_string(strRecvTime)
				# Need to get the last one to receive
				if ((not DSBlockRecvTime.has_key(blockNumber)) or recvTime > convert_time_string(DSBlockRecvTime[blockNumber])):
					DSBlockRecvTime[blockNumber] = strRecvTime

		elif line.find(KEYWORD_TXNPROC_START) != -1:
			blockNumber = get_block_number(line)
			strStartTime = get_time(line)
			startTime = convert_time_string(strStartTime)
			if ((not TxnProcStartTime.has_key(blockNumber)) or startTime < convert_time_string(TxnProcStartTime[blockNumber])):
				TxnProcStartTime[blockNumber] = strStartTime

		elif line.find(KEYWORD_TXNPROC) != -1:
			blockNumber = get_block_number(line)
			if (line.find(KEYWORD_DONE) != -1):
				strEndTime = get_time(line)
				endTime = convert_time_string(strEndTime)
				# Need to get the last finishes to process
				if ((not TxnProcEndTime.has_key(blockNumber)) or endTime > convert_time_string(TxnProcEndTime[blockNumber])):
					TxnProcEndTime[blockNumber] = strEndTime

		elif line.find(KEYWORD_MICON) != -1:
			check_micro_consensus(line)

		elif line.find(KEYWORD_MIBLK) != -1:
			check_miblock_broadcast(line)

		elif line.find(KEYWORD_FBCON) != -1:
			check_final_consensus(line)

		elif line.find(KEYWORD_FLBLK) != -1:
			check_flblk_broadcast(line)

		elif line.find(KEYWORD_IDENT) != -1:
			check_leader(line, nodeId)

		elif line.find(KEYWORD_REWARD) != -1:
			check_node_reward(nodeId, line)

	file.close()
	NodeLastBlock[nodeId] = blockNumber

	global LatestBlockNumber
	if blockNumber > LatestBlockNumber:
		LatestBlockNumber = blockNumber

def convert_time_string(strTime):
	a,b,cd = strTime.split(':')
	c,d = cd.split('.')
	return int(a) * 3600000 + int(b) * 60000 + int(c) * 1000 + int(d)

def printResult(outputFile):	
	DSConsensusStartTimeKeys = DSConsensusStartTime.keys()
	DSConsensusStartTimeValues = DSConsensusStartTime.values()
	DSConsensusEndTimeValues = DSConsensusEndTime.values()

	DSBlockSendTimeValues = DSBlockSendTime.values()
	DSBlockRecvTimeValues = DSBlockRecvTime.values()

	MBConsensusDictKeys = MIConsensusDict.keys()
	MBConsensusDictValues = MIConsensusDict.values()

	TxnProcEndTimeKeys = TxnProcEndTime.keys()
	TxnProcStartTimeValues = TxnProcStartTime.values()
	TxnProcEndTimeValues = TxnProcEndTime.values()

	MIBlockSendTimeValues = MIBlockSendTime.values()
	MIBlockRecvTimeValues = MIBlockRecvTime.values()

	FBConsensusDictKeys = FBConsensusDict.keys()
	FBConsensusDictValues = FBConsensusDict.values()

	FLBlockSendTimeValues = FLBlockSendTime.values()
	FLBlockRecvTimeValues = FLBlockRecvTime.values()

	totalFBBlockNumber = len(FBConsensusDictKeys)
	totalDSBlockNumber = len(DSConsensusEndTimeValues)

	# Write the data column titles
	outputFile.write("Operation   \t Epoch\t Start Time\t     End Time  \tTime Span(us)\n")

	dsIndex = 0
	fbIndex = 0
	mbIndex = 0
	txnIndex = 0
	while fbIndex < totalFBBlockNumber:
		blockNumber = FBConsensusDictKeys[fbIndex]
		if DSConsensusStartTimeKeys[dsIndex] == FBConsensusDictKeys[fbIndex]:
			# Print out PoW time, it is consider from end of last final block to the beginning of new DS block
			if dsIndex > 0 and fbIndex > 0:
				powStartTime = FBConsensusDictValues[fbIndex-1][0].endTime
				powFinishTime = DSConsensusStartTimeValues[dsIndex]
				powTimeSpan = convert_time_string(powFinishTime) - convert_time_string(powStartTime)
				dsConStartTime = convert_time_string(DSConsensusEndTimeValues[dsIndex])				
				powCount = POWCountOfEpoch[blockNumber]
				strPoW = 'POW (%6d)' % powCount
				outputFile.write(OUTPUT_LINE_FORMAT % (strPoW, blockNumber, powStartTime, powFinishTime, powTimeSpan))				

			# Print out DS consensus time
			dsTimeSpan = convert_time_string(DSConsensusEndTimeValues[dsIndex]) - convert_time_string(DSConsensusStartTimeValues[dsIndex])
			outputFile.write(OUTPUT_LINE_FORMAT_WO_RETURN % ('DS Consensus',DSConsensusStartTimeKeys[dsIndex], DSConsensusStartTimeValues[dsIndex], DSConsensusEndTimeValues[dsIndex], dsTimeSpan))

			# Print DS leader information
			outputFile.write('DS Leader   \tNode_%04d\t%s\n' % (DSLeaderNodeId.get(blockNumber), DSLeaderIp.get(blockNumber)))

			# Print out DS block broadcast time
			dsBdcastSpan = convert_time_string(DSBlockRecvTimeValues[dsIndex]) - convert_time_string(DSBlockSendTimeValues[dsIndex])
			outputFile.write(OUTPUT_LINE_FORMAT % ('DSBlk Bdcast', DSConsensusStartTimeKeys[dsIndex], DSBlockSendTimeValues[dsIndex], DSBlockRecvTimeValues[dsIndex], dsBdcastSpan))

		if (txnIndex < len(TxnProcEndTimeKeys)):
			if TxnProcStartTime.has_key(blockNumber) and TxnProcEndTime.has_key(blockNumber):
				txnProcSpan = convert_time_string(TxnProcEndTime.get(blockNumber)) - convert_time_string(TxnProcStartTime.get(blockNumber))
				outputFile.write(OUTPUT_LINE_FORMAT % ('Process Txns', blockNumber, TxnProcStartTime.get(blockNumber), TxnProcEndTime.get(blockNumber), txnProcSpan))
				txnIndex += 1

		if mbIndex < len(MBConsensusDictKeys):
			if (MBConsensusDictKeys[mbIndex] == blockNumber):
				#print(MBConsensusDictValues[mbIndex].items())
				for shardId, mbConsensus in MBConsensusDictValues[mbIndex].items():
					miConName = 'MI Con(%4d)' % shardId
					outputFile.write(OUTPUT_LINE_FORMAT_WO_RETURN % (miConName, mbConsensus.blockNumber, mbConsensus.startTime, mbConsensus.endTime, mbConsensus.timeSpan))

					if (MIConsensusLeader.has_key(blockNumber)):
						miConLeaders = MIConsensusLeader.get(blockNumber)
						if (miConLeaders.has_key(shardId)):
							nodeLeader = miConLeaders.get(shardId)
							outputFile.write("SD Leader   \tNode_%04d\t%s\n" % (nodeLeader.nodeId, nodeLeader.ipAddress))


				miBdcastSpan = convert_time_string(MIBlockRecvTimeValues[mbIndex]) - convert_time_string(MIBlockSendTimeValues[mbIndex])
				outputFile.write(OUTPUT_LINE_FORMAT % ('MIBlk Bdcast', MBConsensusDictKeys[mbIndex], MIBlockSendTimeValues[mbIndex], MIBlockRecvTimeValues[mbIndex], miBdcastSpan))
				mbIndex += 1
			else:
				print("Warning: no complete micro block consensus found for block " + str(FBConsensusDictKeys[fbIndex]))

		for fbConsensus in FBConsensusDictValues[fbIndex]:
			outputFile.write(OUTPUT_LINE_FORMAT % ('FB Consensus', fbConsensus.blockNumber, fbConsensus.startTime, fbConsensus.endTime, fbConsensus.timeSpan))

		flBdcastSpan = convert_time_string(FLBlockRecvTimeValues[fbIndex]) - convert_time_string(FLBlockSendTimeValues[fbIndex])
		outputFile.write(OUTPUT_LINE_FORMAT % ('FLBlk Bdcast', FBConsensusDictKeys[fbIndex], FLBlockSendTimeValues[fbIndex], FLBlockRecvTimeValues[fbIndex], flBdcastSpan))

		fbIndex += 1

		if fbIndex >= totalFBBlockNumber:
			break

		if (dsIndex < len(DSConsensusStartTimeKeys) - 1 and DSConsensusStartTimeKeys[dsIndex + 1] == FBConsensusDictKeys[fbIndex]):
			dsIndex += 1

def print_node_epoch(outputFile):
	NodeLastBlockKeys = NodeLastBlock.keys()
	NodeLastBlockValues = NodeLastBlock.values()
	NodeRewardValues = NodeReward.values()
	#outputFile.write("\n")
	outputFile.write("\nNode_Id  \tLast Block\tTotal Rewards\n")

	for nodeId, blockNumber in NodeLastBlock.items():
		outputFile.write("Node_%04d\t%6d\t%10d\n" %(nodeId, blockNumber, NodeReward.get(nodeId)))
		if LatestBlockNumber - blockNumber > NODE_DROP_OFF_BLOCK_NUMBER:
			print("Node_%04d Drop off already" % nodeId)

def main():
	numargs = len(sys.argv)
	if (numargs < 3):
		print_usage()
	else:
		stateLogPath = sys.argv[1]
		if os.path.exists(stateLogPath) != True:
			print ("Path " + stateLogPath + " not exist!")
			print_usage()
			return

		outputFileName = sys.argv[2]
		outputFile = open(outputFileName, "w+")
		if outputFile.closed:
			print ("Failed to open file " + outputFileName)
			print_usage()
			return

		fileNames = find_files(stateLogPath, STATE_LOG_FILE)
		for fileName in fileNames:
			print("Checking file: " + fileName)
			scan_file(fileName)

		printResult(outputFile)

		print_node_epoch(outputFile)

		outputFile.close()
		print("The result successfully write into file: " + outputFileName)

if __name__ == "__main__":
	main()
