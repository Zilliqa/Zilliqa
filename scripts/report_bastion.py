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

import os
import sys
import fnmatch
import re
import time
import random
import threading
import subprocess
import datetime

RESULT_LOOKUP = dict()
RESULT_DSNODE = dict()

def print_usage():
	print("Usage:\tpython " + sys.argv[0] + "<report title> <lookup pods TXT> <DS guard pods TXT> <webhook URL> <poll interval in mins (0 = run once)>\n")

def getPods(fileName):
	file = open(fileName, "r+")
	result = []

	for line in file:
		result.append(line.strip())
	file.close()

	return result

def getSubset(DSNodes, count):
	subset = [DSNodes[i] for i in sorted(random.sample(list(range(len(DSNodes))), count))]
	return subset

def getLookupData(lookup):
	RESULT_LOOKUP[lookup] = subprocess.check_output(['kubectl','exec',lookup,'--','bash','-c','tac state-00001-log.txt | grep -m1 DSBLK ; tac state-00001-log.txt | grep -m1 "RECVD FLBLK" ; tac state-00001-log.txt | grep -m1 "\\[VCBLK\\] DS"']).strip()

def getDSNodeData(ds):
	RESULT_DSNODE[ds] = subprocess.check_output(['kubectl','exec',ds,'--','bash','-c','tac state-00001-log.txt | grep -m1 "DS PoW"']).strip()

def generateReport(reportname, lookups, dsNodesSubset, webhookURL):

	report_string = "*" + reportname + "*\n"
	report_string += "```"

	# Retrieve data from lookups
	RESULT_LOOKUP.clear()
	threads = []
	for item in lookups:
		threads.append(threading.Thread(target=getLookupData(item)))
	for item in threads:
		item.start()
	for item in threads:
		item.join()

	# 1 = Latest DS time
	# 2 = Latest DS Tx#
	# 9 = Latest DS DS Diff
	# 12 = Latest DS Diff
	# 13 = Tx# time
	# 14 = Tx#
	# 17 = VC time
	# 21 = Latest VC DS
	# 24 = Latest VC Tx

	lookupData = dict()
	for key, val in list(RESULT_LOOKUP.items()):
		lookupnum = (int)(key[key.rfind('-') + 1:])
		rawdatasegments = val.split(' ')

		lookupData[lookupnum] = []

		# 2 = Latest DS Tx#
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[2][rawdatasegments[2].rfind('['):]).group())

		# 9 = Latest DS DS Diff
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[9]).group())

		# 12 = Latest DS Diff
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[12]).group())

		# 14 = Tx#
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[14][rawdatasegments[14].rfind('['):]).group())

		# 21 = Latest VC DS
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[21]).group())

		# 24 = Latest VC Tx
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[24]).group())

		# 1 = Latest DS time
		lookupData[lookupnum].append(rawdatasegments[1])

		# 13 = Tx# time
		lookupData[lookupnum].append(rawdatasegments[13])

		# 17 = VC time
		lookupData[lookupnum].append(rawdatasegments[17])

	# Get the lookup with latest Tx and VC
	latestIndex3Entry = 0
	latestIndex5Entry = 0
	for lookup in lookupData:
		if (lookupData[lookup][3] > lookupData[latestIndex3Entry][3]):
			latestIndex3Entry = lookup
		if (lookupData[lookup][5] > lookupData[latestIndex5Entry][5]):
			latestIndex5Entry = lookup

	# Generate LOOKUPS table in report
	thetime = lookupData[latestIndex3Entry][7]
	report_string += "LOOKUP LATEST TXBLK (" + (str)((datetime.datetime.now() - datetime.datetime.strptime(thetime, "%y-%m-%dT%H:%M:%S.%f")).seconds) + " sec ago)\n"
	report_string += "================================\n"

	MAXPERCOL = 5

	num_columns = len(lookups) / MAXPERCOL
	if ((len(lookups) % MAXPERCOL) > 0):
		num_columns = num_columns + 1
	for x in range(0, num_columns):
		report_string += "#   Tx#     "
	report_string += "\n"

	for line_index in range(0, MAXPERCOL):
		if (line_index in lookupData):
			report_string += str(line_index).ljust(4) + lookupData[line_index][3].ljust(8)
		else:
			break

		for next_col in range(1, num_columns):
			if ((line_index + (next_col*MAXPERCOL)) in lookupData):
				report_string += str(line_index + (next_col*MAXPERCOL)).ljust(4) + lookupData[line_index + (next_col*MAXPERCOL)][3].ljust(8)

		report_string += "\n"

	report_string += "\n"

	# Generate LATEST DS BLOCK table in report
	thetime = lookupData[latestIndex3Entry][6]
	report_string += "LATEST DS BLOCK (" + (str)((datetime.datetime.now() - datetime.datetime.strptime(thetime, "%y-%m-%dT%H:%M:%S.%f")).seconds / 60) + " min ago)\n"
	report_string += "================================\n"
	report_string += "Tx#     = " + lookupData[latestIndex3Entry][0] + "\n"
	report_string += "DS Diff = " + lookupData[latestIndex3Entry][1] + "\n"
	report_string += "Diff    = " + lookupData[latestIndex3Entry][2] + "\n"

	report_string += "\n"

	# Generate LATEST VC BLOCK table in report
	thetime = lookupData[latestIndex5Entry][8]
	report_string += "LATEST VC BLOCK (" + (str)((datetime.datetime.now() - datetime.datetime.strptime(thetime, "%y-%m-%dT%H:%M:%S.%f")).seconds / 60) + " min ago)\n"
	report_string += "================================\n"
	report_string += "DS#     = " + lookupData[latestIndex5Entry][4] + "\n"
	report_string += "Tx#     = " + lookupData[latestIndex5Entry][5] + "\n"

	# Retrieve data from DS nodes
	RESULT_DSNODE.clear()
	threads = []
	for item in dsNodesSubset:
		threads.append(threading.Thread(target=getDSNodeData(item)))
	for item in threads:
		item.start()
	for item in threads:
		item.join()

	report_string += "\n"

	# Generate DS NODES: LATEST PoW COUNTS table in report
	report_string += "LATEST PoW COUNTS (RANDOM NODES)\n"
	report_string += "================================\n"
	report_string += "#     Tx#     DSPoW    PoW\n"

	# 2 = DS Tx#
	# 6 = DS PoW
	# 9 = PoW

	dsNodeData = dict()
	for key, val in list(RESULT_DSNODE.items()):
		dsnum = (int)(key[key.rfind('-') + 1:])
		rawdatasegments = val.split(' ')

		dsNodeData[dsnum] = []

		# 2 = DS Tx#
		dsNodeData[dsnum].append(re.search(r'\d+', rawdatasegments[2][rawdatasegments[2].rfind('['):]).group())

		# 6 = DS PoW
		dsNodeData[dsnum].append(re.search(r'\d+', rawdatasegments[6]).group())
	
		# 9 = PoW
		dsNodeData[dsnum].append(re.search(r'\d+', rawdatasegments[9]).group())

	keylist = list(dsNodeData.keys())
	keylist.sort()
	for key in keylist:
		report_string += str(key).ljust(6) + dsNodeData[key][0].ljust(8) + dsNodeData[key][1].ljust(9) + dsNodeData[key][2] + "\n"

	report_string += "```"

	subprocess.check_output(['curl','-X','POST','-H','Content-type: application/json','--data','{"text":"' + report_string + '"}',webhookURL])

	return

def main():
	numargs = len(sys.argv)
	if (numargs < 6):
		print_usage()
	else:
		REPORTNAME = sys.argv[1]
		LOOKUPSTXT = sys.argv[2]
		DSNODESTXT = sys.argv[3]
		WEBHOOKURL = sys.argv[4]
		POLLINMINS = (int)(sys.argv[5])
		Lookups = getPods(LOOKUPSTXT)
		DSNodes = getPods(DSNODESTXT)
		if (POLLINMINS == 0):
			DSNodesSubset = getSubset(DSNodes,5)
			generateReport(REPORTNAME, Lookups, DSNodesSubset, WEBHOOKURL)
		else:
			while True:
				DSNodesSubset = getSubset(DSNodes,5)
				generateReport(REPORTNAME, Lookups, DSNodesSubset, WEBHOOKURL)
				time.sleep(POLLINMINS * 60)

if __name__ == "__main__":
	main()
