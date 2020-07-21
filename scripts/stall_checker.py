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
import tempfile
import threading
import subprocess
import datetime

RESULT_LOOKUP = dict()

def print_usage():
	print("Usage:\tpython " + sys.argv[0] + "<report title> <lookup pods TXT> <webhook URL> <stall tolerance in mins> <poll interval in mins (0 = run once)>\n")

def getPods(fileName):
	file = open(fileName, "r+")
	result = []

	for line in file:
		result.append(line.strip())
	file.close()

	return result

def getLookupData(lookup):
	RESULT_LOOKUP[lookup] = subprocess.check_output(['kubectl','exec',lookup,'--','bash','-c','tac state-00001-log.txt | grep -m1 "RECVD FLBLK"']).strip()

def generateReport(reportname, lookups, webhookURL, stalInMins):

	# Retrieve data from lookups
	RESULT_LOOKUP.clear()
	threads = []
	for item in lookups:
		threads.append(threading.Thread(target=getLookupData(item)))
	for item in threads:
		item.start()
	for item in threads:
		item.join()

	stallDetected = False

	# 1 = Latest Tx# time
	# 2 = Latest Tx#

	lookupData = dict()
	thetimenow = datetime.datetime.now()
	for key, val in list(RESULT_LOOKUP.items()):
		lookupnum = (int)(key[key.rfind('-') + 1:])
		rawdatasegments = val.split(' ')

		lookupData[lookupnum] = []

		# 1 = Latest DS time
		lookupData[lookupnum].append(rawdatasegments[1])

		# 2 = Latest DS Tx#
		lookupData[lookupnum].append(re.search(r'\d+', rawdatasegments[2][rawdatasegments[2].rfind('['):]).group())

		thetime = lookupData[lookupnum][0]
		thetimediff = (thetimenow - datetime.datetime.strptime(thetime, "%y-%m-%dT%H:%M:%S.%f")).seconds

		if (thetimediff > (stalInMins * 60)):
			stallDetected = True

		lookupData[lookupnum].append(thetimediff)
		lookupData[lookupnum].append(thetimediff > (stalInMins * 60))

	# End checking if all lookups still within tolerance
	if (stallDetected == False):
		return

	# Else, send alert
	report_string = "*" + reportname + "*\n"
	report_string += "```"

	# Generate LOOKUPS table in report
	report_string += "LOOKUP LATEST TXBLK\n"
	report_string += "=======================\n"
	report_string += "#   Tx#     Received\n"

	for lookup_index in lookupData:
		report_string += str(lookup_index).ljust(4) + lookupData[lookup_index][1].ljust(8) + str(lookupData[lookup_index][2] / 60) + " min ago"
		if (lookupData[lookup_index][3] == True):
			report_string += "   <--"
		report_string += "\n"

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
		WEBHOOKURL = sys.argv[3]
		STALLINMINS= (int)(sys.argv[4])
		POLLINMINS = (int)(sys.argv[5])
		Lookups = getPods(LOOKUPSTXT)
		if (POLLINMINS == 0):
			generateReport(REPORTNAME, Lookups, WEBHOOKURL, STALLINMINS)
		else:
			while True:
				generateReport(REPORTNAME, Lookups, WEBHOOKURL, STALLINMINS)
				time.sleep(POLLINMINS * 60)

if __name__ == "__main__":
	main()
