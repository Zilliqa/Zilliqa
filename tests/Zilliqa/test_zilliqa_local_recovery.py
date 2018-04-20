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

NODE_LISTEN_PORT = 5001
LOCAL_RUN_FOLDER = './local_run/'

def print_usage():
	print ("Testing multiple Zilliqa nodes in local machine\n"
		"===============================================\n"
		"Usage:\n\tpython " + sys.argv[0] + " [command] [command parameters]\n"
		"Available commands:\n"
		"\tTest Execution:\n"
		"\t\tsetup [num-nodes]           - Set up the nodes\n"
		"\t\tstart [num-nodes]           - Start node processes\n"
		"\t\tconnect                     - Connect everyone\n"
		"\t\tconnect [num-nodes]         - Connect first num-nodes nodes\n"
		"\t\tstop                        - Stop node processes\n"
		"\t\tclean                       - Remove test output files (e.g., logs)\n"
		"\t\tsendcmd [nodenum] [hex msg] - Send hex msg to port\n"
		"\t\tsendcmdrandom [nodenum] [msgsize] - Send msg to port\n"
		"\t\tstartpow1 [nodenum] [ds count] [blocknum] [diff] [rand1] [rand2] - Send STARTPOW1 to node\n"
		"\t\tcreatetx [nodenum] [from] [to] [amount] - Send CREATETRANSACTION to node\n"
		"\t\tdelete                      - Delete the set-up nodes\n"
		"\t\tsendtxn [port]              - Delete dummy txn at the interval of 1 per seconds\n")

def main():
	numargs = len(sys.argv)
	if (numargs < 2):
		print_usage()
	else:
		command = sys.argv[1]
		if (command == 'setup'):
			print_usage() if (numargs != 3) else run_setup(numnodes=int(sys.argv[2]), printnodes=True)
		elif (command == 'start'):
			print_usage() if (numargs != 3) else run_start(numdsnodes=int(sys.argv[2]))
		elif (command == 'connect'):
			if (numargs == 2):
				run_connect(numnodes=0)
			elif (numargs == 3):
				run_connect(numnodes=int(sys.argv[2]))
			else:
				print_usage()
		elif (command == 'stop'):
			print_usage() if (numargs != 2) else run_stop()
		elif (command == 'clean'):
			print_usage() if (numargs != 2) else run_clean()
		elif (command == 'sendcmd'):
			print_usage() if (numargs != 4) else run_sendcmd(nodenum=int(sys.argv[2]), msg=sys.argv[3])
		elif (command == 'sendcmdrandom'):
			print_usage() if (numargs != 4) else run_sendcmdrandom(nodenum=int(sys.argv[2]), msg_size=sys.argv[3])
		elif (command == 'startpow1'):
			print_usage() if (numargs != 8) else run_startpow1(nodenum=int(sys.argv[2]), dscount=int(sys.argv[3]), blocknum=sys.argv[4], diff=sys.argv[5], rand1=sys.argv[6], rand2=sys.argv[7])
		elif (command == 'createtx'):
			print_usage() if (numargs != 6) else run_createtx(nodenum=int(sys.argv[2]), fromnode=int(sys.argv[3]), tonode=int(sys.argv[4]), amount=int(sys.argv[5]))
		elif (command == 'delete'):
			print_usage() if (numargs != 2) else run_delete()
		elif (command == 'sendtxn'):
			print_usage() if (numargs != 3) else run_sendtxn(portnum=int(sys.argv[2]))
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
	if os.path.exists(LOCAL_RUN_FOLDER) != True :
		# shutil.rmtree(LOCAL_RUN_FOLDER)
		os.makedirs(LOCAL_RUN_FOLDER)
	for x in range(0, numnodes):
		testsubdir = LOCAL_RUN_FOLDER + 'node_' + str(x+1).zfill(4)
		if os.path.exists(testsubdir) != True :
			os.makedirs(testsubdir)
		shutil.copyfile('./build/tests/Zilliqa/zilliqa', testsubdir + '/zilliqa')
		shutil.copyfile('./build/tests/Zilliqa/sendcmd', testsubdir + '/sendcmd')
		shutil.copyfile('./build/tests/Zilliqa/sendtxn', testsubdir + '/sendtxn')

		st = os.stat(testsubdir + '/zilliqa')
		os.chmod(testsubdir + '/zilliqa', st.st_mode | stat.S_IEXEC)
		st = os.stat(testsubdir + '/sendcmd')
		os.chmod(testsubdir + '/sendcmd', st.st_mode | stat.S_IEXEC)
		st = os.stat(testsubdir + '/sendtxn')
		os.chmod(testsubdir + '/sendtxn', st.st_mode | stat.S_IEXEC)

	if printnodes:
		testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
		count = len(testfolders_list)
		for x in range(0, count):
			print '[Node ' + str(x + 1).ljust(3) + '] [Port ' + str(NODE_LISTEN_PORT + x) + '] ' + LOCAL_RUN_FOLDER + testfolders_list[x]

def run_start(numdsnodes):
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)
	keypairs = []

	# Generate keypairs (sort by public key)
	for x in range(0, count):
		process = Popen(["./build/tests/Zilliqa/genkeypair"], stdout=PIPE)
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
		if (x < numdsnodes):
			peer = ET.SubElement(nodes, "peer")
			ET.SubElement(peer, "pubk").text = keypair[0]
			ET.SubElement(peer, "ip").text = '127.0.0.1'
			ET.SubElement(peer, "port").text = str(NODE_LISTEN_PORT + x)
	keys_file.close()

	# Create config.xml with pubkey and IP info of all DS nodes
	tree = ET.ElementTree(nodes)
	tree.write("config.xml")

	# Launch node Zilliqa process
	for x in range(0, count):
		keypair = keypairs[x].split(" ")
		if (x < numdsnodes):
			shutil.copyfile('config.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/config.xml')
			shutil.copyfile('constants_local.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml')
			os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; $(pwd)/zilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + '127.0.0.1' +' ' + str(NODE_LISTEN_PORT + x) + ' 1 0 1 > ./error_log_zilliqa 2>&1 &')
		else:
			shutil.copyfile('constants_local.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml')
			os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; $(pwd)/zilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + '127.0.0.1' +' ' + str(NODE_LISTEN_PORT + x) + ' 0 0 1 > ./error_log_zilliqa 2>&1 &')

def run_connect(numnodes):
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)
	if ((numnodes == 0) or (numnodes > count)):
		numnodes = count

	# Load the keypairs
	keypairs = []
	with open(LOCAL_RUN_FOLDER + 'keys.txt') as f:
		keypairs = f.readlines()
	keypairs = [x.strip() for x in keypairs]

	# Connect nodes (exchange hello messages)
	edges = set()
	for x in range(0, numnodes):
		connect_cmd = 'cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; ulimit -Sc unlimited; ulimit -Hc unlimited;  ./sendcmd ' + str(NODE_LISTEN_PORT + x) + ' addpeers'
		has_peers_to_connect = False
		for y in range(x + 1, numnodes):
			index = y
			if ((x + 1, index + 1) in edges):
				continue
			elif ((index + 1, x + 1) in edges):
				continue
			elif (x == index):
				continue
			else:
				has_peers_to_connect = True
				keypair = keypairs[index].split(" ")
				print ('connecting node ' + str(x + 1) + ' (port ' + str(NODE_LISTEN_PORT + x) + ') to node ' + str(index + 1) + ' (' + str(NODE_LISTEN_PORT + index) + ')')
				connect_cmd = connect_cmd + ' ' + keypair[0] + ' 127.0.0.1 ' + str(NODE_LISTEN_PORT + index)
				if (x < index):
					edges.add((x + 1, index + 1))
				else:
					edges.add((index + 1, x + 1))
		if has_peers_to_connect:
			os.system(connect_cmd + ' &')
			time.sleep(1)

	print 'Total num of edges connected: ' + str(len(edges))

def run_stop():
	os.system('killall zilliqa')
	os.system('killall sendtxn')
	if os.path.exists(LOCAL_RUN_FOLDER):
		testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
		count = len(testfolders_list)
		for x in range(0, count):
			os.system('fuser -k ' + str(NODE_LISTEN_PORT + x) + '/tcp')

def run_clean():
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)
	run_setup(count, False)

def run_sendcmd(nodenum, msg):
	os.system('build/tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT + nodenum - 1) + ' cmd ' + msg)
	
def run_sendcmdrandom(nodenum, msg_size):
	# msg = "000400" + 'A' * msg_size * 2
	# os.system('build/tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT + nodenum - 1) + ' cmd ' + msg)
	os.system('build/tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT) + ' broadcast ' + msg_size)

def run_startpow1(nodenum, dscount, blocknum, diff, rand1, rand2):
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)

	# Load the keypairs
	keypairs = []
	with open(LOCAL_RUN_FOLDER + 'keys.txt') as f:
		keypairs = f.readlines()
	keypairs = [x.strip() for x in keypairs]

	# Assemble the STARTPOW1 message
	startpow1_cmd = 'build/tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT + nodenum - 1) + ' cmd 0200' + blocknum + diff + rand1 + rand2
	for x in range(0, dscount):
		keypair = keypairs[x].split(" ")
		startpow1_cmd = startpow1_cmd + keypair[0] + '0000000000000000000000000100007F' + "{0:0{1}x}".format(NODE_LISTEN_PORT + x, 8)

	# Send to node
	os.system(startpow1_cmd)

def run_sendtxn(portnum):
	os.system('build/tests/Zilliqa/sendtxn ' + str(portnum) + ' &')

def run_delete():
	if (os.path.exists(LOCAL_RUN_FOLDER)):
		shutil.rmtree(LOCAL_RUN_FOLDER)

if __name__ == "__main__":
	main()