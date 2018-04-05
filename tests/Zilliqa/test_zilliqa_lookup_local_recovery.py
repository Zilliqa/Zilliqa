#!/usr/bin/env python

# Copyright (c) 2018 Zilliqa 
# This source code is being disclosed to you solely for the purpose of your participation in 
# testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
# the protocols and algorithms that are programmed into, and intended by, the code. You may 
# not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
# including modifying or publishing the code (or any part of it), and developing or forming 
# another public or private blockchain network. This source code is provided 'as is' and no 
# warranties are given as to title or non-infringement, merchantability or fitness for purpose 
# and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
# Some programs in this code are governed by the GNU General Public License v3.0 (available at 
# https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that are governed by 
# GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
# and which include a reference to GPLv3 in their program files.


import subprocess
import os
import sys
import shutil
import stat
import time 

from subprocess import Popen, PIPE
import xml.etree.cElementTree as ET

NODE_LISTEN_PORT = 4001
LOCAL_RUN_FOLDER = './lookup_local_run/'

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
	os.system('killall lzilliqa')
	if os.path.exists(LOCAL_RUN_FOLDER) != True:
		# shutil.rmtree(LOCAL_RUN_FOLDER)
		os.makedirs(LOCAL_RUN_FOLDER)
	for x in range(0, numnodes):
		testsubdir = LOCAL_RUN_FOLDER + 'node_' + str(x+1).zfill(4)
		if os.path.exists(testsubdir) != True :
			os.makedirs(testsubdir)
		shutil.copyfile('./tests/Zilliqa/zilliqa', testsubdir + '/lzilliqa')

		st = os.stat(testsubdir + '/lzilliqa')
		os.chmod(testsubdir + '/lzilliqa', st.st_mode | stat.S_IEXEC)

	if printnodes:
		testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
		count = len(testfolders_list)
		for x in range(0, count):
			print '[Node ' + str(x + 1).ljust(3) + '] [Port ' + str(NODE_LISTEN_PORT + x) + '] ' + LOCAL_RUN_FOLDER + testfolders_list[x]

def run_start():
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)
	keypairs = []

	# Generate keypairs (sort by public key)
	for x in range(0, count):
		process = Popen(["./tests/Zilliqa/genkeypair"], stdout=PIPE)
		(output, err) = process.communicate()
		exit_code = process.wait()
		keypairs.append(output)
	keypairs.sort()

	nodes = ET.Element("nodes")

	# Store sorted keys list in text file
	keys_file = open(LOCAL_RUN_FOLDER + 'keys.txt', "w")
	for x in range(0, count):
		keys_file.write(keypairs[x] + '\n')
		keypair = keypairs[x].split(" ")
		if (x < count):
			peer = ET.SubElement(nodes, "peer")
			ET.SubElement(peer, "pubk").text = keypair[0]
			ET.SubElement(peer, "ip").text = '127.0.0.1'
			ET.SubElement(peer, "port").text = str(NODE_LISTEN_PORT + x)
	keys_file.close()

	# Create config.xml with pubkey and IP info of all DS nodes
	tree = ET.ElementTree(nodes)
	tree.write("config.xml")

	# Store sorted keys list in text file
	keys_file = open(LOCAL_RUN_FOLDER + 'keys.txt', "w")
	for x in range(0, count):
		keys_file.write(keypairs[x] + '\n')
		shutil.copyfile('config.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/config.xml')
		shutil.copyfile('constants_local.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml')
	keys_file.close()

	# Launch node zilliqa process
	for x in range(0, count):
		keypair = keypairs[x].split(" ")
		os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; $(pwd)/lzilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + '127.0.0.1' +' ' + str(NODE_LISTEN_PORT + x) + ' 0 0 1 > ./error_log_zilliqa 2>&1 &')

if __name__ == "__main__":
	main()
