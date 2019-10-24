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

from subprocess import Popen, PIPE

NODE_LISTEN_PORT = 6001
LOCAL_RUN_FOLDER = './late_local_run/'

def print_usage():
	print ("Testing multiple Zilliqa nodes in local machine\n"
		"===============================================\n"
		"Usage:\n\tpython " + sys.argv[0] + " [command] [command parameters]\n"
		"Available commands:\n"
		"\tTest Execution:\n"
		"\t\tsetup [num-nodes]           - Set up the nodes\n"
		"\t\tstart                       - Start node processes\n")

def main():
	numargs = len(sys.argv)
	if (numargs < 2):
		print_usage()
	else:
		command = sys.argv[1]
		if (command == 'setup'):
			print_usage() if (numargs != 3) else run_setup(numnodes=int(sys.argv[2]), printnodes=True)
		elif (command == 'start'):
			print_usage() if (numargs != 2) else run_start()
		else:
			print_usage()

# ================
# Helper Functions
# ================

def get_immediate_subdirectories(a_dir):
	subdirs = [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]
	subdirs.sort()
	return subdirs

# ========================
# Test Execution Functions
# ========================

def run_setup(numnodes, printnodes):
	if (os.path.exists(LOCAL_RUN_FOLDER)):
		shutil.rmtree(LOCAL_RUN_FOLDER)
	os.makedirs(LOCAL_RUN_FOLDER)
	for x in range(0, numnodes):
		testsubdir = LOCAL_RUN_FOLDER + 'node_' + str(x+1).zfill(4)
		os.makedirs(testsubdir)
		shutil.copyfile('./tests/Zilliqa/zilliqa', testsubdir + '/latezilliqa')

		st = os.stat(testsubdir + '/latezilliqa')
		os.chmod(testsubdir + '/latezilliqa', st.st_mode | stat.S_IEXEC)

	if printnodes:
		testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
		count = len(testfolders_list)
		for x in range(0, count):
			print ('[Node ' + str(x + 1).ljust(3) + '] [Port ' + str(NODE_LISTEN_PORT + x) + '] ' + LOCAL_RUN_FOLDER + testfolders_list[x])

def run_start():
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)
	keypairs = []

	# Generate keypairs (sort by public key)
	for x in range(0, count):
		process = Popen(["./tests/Zilliqa/genkeypair"], stdout=PIPE, universal_newlines=True)
		(output, err) = process.communicate()
		exit_code = process.wait()
		keypairs.append(output.strip())
	keypairs.sort()

	# Store sorted keys list in text file
	keys_file = open(LOCAL_RUN_FOLDER + 'keys.txt', "w")
	for x in range(0, count):
		keys_file.write(keypairs[x] + '\n')
		shutil.copyfile('dsnodes.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/dsnodes.xml')
		shutil.copyfile('constants_local.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml')
	keys_file.close()

	
	# Launch node zilliqa process
	for x in range(0, count):
		keypair = keypairs[x].split(" ")
		os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; ./latezilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + 'NAT' + ' '  + str(NODE_LISTEN_PORT + x) + ' 0 1 0 > ./error_log_zilliqa 2>&1 &')
                os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; ./latezilliqa ' + ' --privk ' + keypair[1] + ' --pubk ' + keypair[0] + ' --address ' + 'NAT' + ' --port '  + str(NODE_LISTEN_PORT + x) + '--synctype 1 ' + '> ./error_log_zilliqa 2>&1 &')


if __name__ == "__main__":
	main()
