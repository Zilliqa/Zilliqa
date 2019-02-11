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
import subprocess
import os
import sys
import shutil
import stat
import time 
import random

from subprocess import Popen, PIPE


def print_usage():
	print ("Testing signmultisig in local machine\n"
		"===============================================\n"
		"Usage:\n\tpython " + sys.argv[0] + " [command] [command parameters]\n"
		"Available commands:\n"
		"\tTest Execution:\n"
		"\t\tsetup [path-to-[signmultisig,genkeypair]-binaries, new-folder-path/name-for-test-files]\t- Set up the test\n"
		"\t\tstart [path-to-verifyMultiSig, new-folder-path/name-for-test-files]\t\t\t- Start node processes\n")

def main():
	numargs = len(sys.argv)
	if (numargs < 4):
		print_usage()
		sys.exit(1)
	else:
		command = sys.argv[1]
		if (command == 'setup'):
			if (numargs != 4):
				print_usage()
				sys.exit(1)
			else: 
				run_setup(sys.argv[2], sys.argv[3])
		elif (command == 'start'):
			if (numargs != 4):
				print_usage()
				sys.exit(1)
			else: 
				run_start(sys.argv[2], sys.argv[3])
		else:
			print_usage()
			sys.exit(1)


# ================
# Helper Functions
# ================

def generateRandomMessage():
	message_len = random.randint(1,100)
	message = ""
	for ch in range(message_len):
		message += str(random.randint(1,9))
	return message

def appendSlash(paths):
	paths_new = []
	for p in paths:
		if p[-1] == '/':
			paths_new.append(p)
		else:
			paths_new.append(p + '/')
	return paths_new

def keysToFile(key_file, keys, key_type):
	file_ = open(key_file, "w")
	for k in keys:
		file_.write(k[key_type] + '\n')
	file_.close()


# ========================
# Test Execution Functions
# ========================

def run_setup(CMD_BIN_PATH, LOCAL_TESTRUN_FOLDER):
	CMD_BIN_PATH, LOCAL_TESTRUN_FOLDER = appendSlash([CMD_BIN_PATH,LOCAL_TESTRUN_FOLDER])
	# Recreates test execution folder
	if LOCAL_TESTRUN_FOLDER != './':
		if (os.path.exists(LOCAL_TESTRUN_FOLDER)):
			shutil.rmtree(LOCAL_TESTRUN_FOLDER)
		os.makedirs(LOCAL_TESTRUN_FOLDER)
		
	# Copies binaries under test into test execution location
	for b in ['genkeypair', 'signmultisig']:
		shutil.copyfile(CMD_BIN_PATH + b, LOCAL_TESTRUN_FOLDER  + b)
		st = os.stat(CMD_BIN_PATH + b)
		os.chmod(LOCAL_TESTRUN_FOLDER + b, st.st_mode | stat.S_IEXEC)

def run_start(VERIFYMULTISIG_PATH, LOCAL_TESTRUN_FOLDER):
	VERIFYMULTISIG_PATH, LOCAL_TESTRUN_FOLDER = appendSlash([VERIFYMULTISIG_PATH, LOCAL_TESTRUN_FOLDER])
	genkeypair = LOCAL_TESTRUN_FOLDER + "genkeypair"
	signmultisig = LOCAL_TESTRUN_FOLDER + "signmultisig"
	verifymultisig = VERIFYMULTISIG_PATH + "VerifyMultiSignature"
	message = generateRandomMessage()
	keypairs_num = random.randint(1,10)
	keypairs = []

	# Generate keypairs (sort by public key)
	for x in range(0, keypairs_num):
		process = Popen([ genkeypair ], stdout=PIPE)
		(output, err) = process.communicate()
		exit_code = process.wait()
		keys = output.strip().split(" ")
		keypairs.append({'pubk':keys[0], 'privk':keys[1]})

	# Store keys list in text files
	pubk_file = LOCAL_TESTRUN_FOLDER + 'pubkeys.txt'
	privk_file = LOCAL_TESTRUN_FOLDER + 'privkeys.txt'
	keysToFile(pubk_file, keypairs, 'pubk')
	keysToFile(privk_file, keypairs, 'privk')		
	
	# Generate signature of a message with signmultisig
        process = Popen([ signmultisig, "-m", message, "-i", privk_file, "-u", pubk_file ], stdout=PIPE)
        (output, err) = process.communicate()
        exit_code = process.wait()
        signature = output.strip()

	# Verify signature of a message with VerifyMultiSignature
        process = Popen([ verifymultisig, "-m", message, "-s", signature, "-u", pubk_file ], stdout=PIPE)
        (output, err) = process.communicate()
        exit_code = process.wait()
        ret_status = process.returncode

	if ret_status == 0:
		print("Test passed")
	else:
		print("Test failed")
		sys.exit(1)		

if __name__ == "__main__":
	main()
