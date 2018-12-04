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
KEYWORD_MBCON = '[MICON]'
KEYWORD_FBCON = '[FBCON]'
KEYWORD_FLBLK = '[FLBLK]'
KEYWORD_BEGIN = 'BGIN'
KEYWORD_DONE = 'DONE'
KEYWORD_SEND = 'SENDING'
KEYWORD_RECV = 'RECEIVED'

OUTPUT_LINE_FORMAT = "%s\t%6d\t%s\t%s\t%d\n"

class Consensus:
	blockNumber = 0
	index = 0
	name = ''
	startTime = ''
	endTime = ''
	timeSpan = 0

DSConsensusStartTime = SortedDict()
DSConsensusEndTime = SortedDict()

DSBlockSendTime = SortedDict()
DSBlockRecvTime = SortedDict()

MBConsensusDict = SortedDict()

FBConsensusDict = SortedDict()
FLBlockSendTime = SortedDict()
FLBlockRecvTime = SortedDict()


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
	m = re.search(r'[\d]+:[\d]+:[\d]+:[ ]*[\d]+', line)
	if m != None:
		return m.group(0)
	else:
		return ""

def get_block_number(line):
	m = re.search(r'[[\d]+]', line)
	blockNumber = m.group(0)
	blockNumber = blockNumber[1:len(blockNumber)-1]
	return int(blockNumber)

def scan_file(fileName):
	file = open(fileName, "r+")
	mbConsensusStartTime = ''
	fbConsensusStartTime = ''
	for line in file:
		if line.find(KEYWORD_DSCON) != -1:
			blockNumber = get_block_number(line)
			if line.find(KEYWORD_BEGIN) != -1:
				DSConsensusStartTime[blockNumber] = get_time(line)				
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
		elif line.find(KEYWORD_MBCON) != -1:
			blockNumber = get_block_number(line)
			if line.find(KEYWORD_BEGIN) != -1:
				mbConsensusStartTime = get_time(line)
			elif line.find(KEYWORD_DONE) != -1:
				mbConsensus = Consensus()
				mbConsensus.blockNumber = blockNumber
				mbConsensus.startTime = mbConsensusStartTime
				mbConsensus.endTime = get_time(line)
				mbConsensus.timeSpan = convert_time_string(mbConsensus.endTime) - convert_time_string(mbConsensus.startTime)
				MBConsensusDict.setdefault(blockNumber, []).append(mbConsensus)
		elif line.find(KEYWORD_FBCON) != -1:
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
		elif line.find(KEYWORD_FLBLK) != -1:
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
	file.close()

def convert_time_string(strTime):
	a,b,c,d = strTime.split(':')
	return int(a)*3600000 + int(b) * 60000 + int(c) * 1000 + int(d)

def printResult(outputFile):	
	DSConsensusStartTimeKeys = DSConsensusStartTime.keys()
	DSConsensusStartTimeValues = DSConsensusStartTime.values()
	DSConsensusEndTimeValues = DSConsensusEndTime.values()

	DSBlockSendTimeValues = DSBlockSendTime.values()
	DSBlockRecvTimeValues = DSBlockRecvTime.values()

	MBConsensusDictKeys = MBConsensusDict.keys()
	MBConsensusDictValues = MBConsensusDict.values()

	FBConsensusDictKeys = FBConsensusDict.keys()
	FBConsensusDictValues = FBConsensusDict.values()

	FLBlockSendTimeValues = FLBlockSendTime.values()
	FLBlockRecvTimeValues = FLBlockRecvTime.values()

	totalFBBlockNumber = len(FBConsensusDictKeys)
	totalDSBlockNumber = len(DSConsensusEndTimeValues)
	dsIndex = 0
	fbIndex = 0
	mbIndex = 0
	while fbIndex < totalFBBlockNumber:
		if DSConsensusStartTimeKeys[dsIndex] == FBConsensusDictKeys[fbIndex]:
			# Print out PoW time, it is consider from end of last final block to the beginning of new DS block
			if dsIndex > 0 and fbIndex > 0:
				powStartTime = FBConsensusDictValues[fbIndex-1][0].endTime
				powFinishTime = DSConsensusStartTimeValues[dsIndex]
				powTimeSpan = convert_time_string(powFinishTime) - convert_time_string(powStartTime)
				dsConStartTime = convert_time_string(DSConsensusEndTimeValues[dsIndex])
				#outputFile.write("POW         \t" + str(DSConsensusStartTimeKeys[dsIndex]) + "\t" + powStartTime + "\t" + powFinishTime + "\t" + str(powTimeSpan) + "\n")
				outputFile.write(OUTPUT_LINE_FORMAT % ('POW         ', DSConsensusStartTimeKeys[dsIndex], powStartTime, powFinishTime, powTimeSpan))

			# Print out DS consensus time
			dsTimeSpan = convert_time_string(DSConsensusEndTimeValues[dsIndex]) - convert_time_string(DSConsensusStartTimeValues[dsIndex])
			#outputFile.write("DS Consensus\t" + str(DSConsensusStartTimeKeys[dsIndex]) + "\t" + DSConsensusStartTimeValues[dsIndex] + "\t" + DSConsensusEndTimeValues[dsIndex] + "\t" + str(dsTimeSpan) + "\n")
			outputFile.write(OUTPUT_LINE_FORMAT % ('DS Consensus',DSConsensusStartTimeKeys[dsIndex], DSConsensusStartTimeValues[dsIndex], DSConsensusEndTimeValues[dsIndex], dsTimeSpan))

			# Print out DS block broadcast time
			dsBdcastSpan = convert_time_string(DSBlockRecvTimeValues[dsIndex]) - convert_time_string(DSBlockSendTimeValues[dsIndex])
			#outputFile.write("DS Bdcast   \t" + str(DSConsensusStartTimeKeys[dsIndex]) + "\t" + DSBlockSendTimeValues[dsIndex] + "\t" + DSBlockRecvTimeValues[dsIndex] + "\t" + str(dsBdcastSpan) + "\n")
			outputFile.write(OUTPUT_LINE_FORMAT % ('DSBlk Bdcast', DSConsensusStartTimeKeys[dsIndex], DSBlockSendTimeValues[dsIndex], DSBlockRecvTimeValues[dsIndex], dsBdcastSpan))

		if mbIndex < len(MBConsensusDictKeys):
			if (MBConsensusDictKeys[mbIndex] == FBConsensusDictKeys[fbIndex]):
				for mbConsensus in MBConsensusDictValues[mbIndex]:
					#mbTimeSpan = convert_time_string(mbConsensus.endTime) - convert_time_string(mbConsensus.startTime)
					#outputFile.write("MB Consensus\t" + str(mbConsensus.blockNumber) + "\t" + mbConsensus.startTime + "\t" + mbConsensus.endTime + "\t" + str(mbTimeSpan) + "\n")
					outputFile.write(OUTPUT_LINE_FORMAT % ('MB Consensus', mbConsensus.blockNumber, mbConsensus.startTime, mbConsensus.endTime, mbConsensus.timeSpan))
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

		outputFile.close()
		print("The result successfully write into file: " + outputFileName)

if __name__ == "__main__":
	main()
