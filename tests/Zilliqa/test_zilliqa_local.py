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
		"\t\tstartpow [nodenum] [ds count] [blocknum] [diff] [rand1] [rand2] - Send STARTPOW to node\n"
		"\t\tcreatetx [nodenum] [from] [to] [amount] - Send CREATETRANSACTION to node\n"
		"\t\tdelete                      - Delete the set-up nodes\n")

def main():
	numargs = len(sys.argv)
	if (numargs < 2):
		print_usage()
	else:
		command = sys.argv[1]
		if (command == 'setup'):
			print_usage() if (numargs != 3) else run_setup(numnodes=int(sys.argv[2]), printnodes=True)
		elif(command == 'prestart'):
			print_usage() if (numargs != 3) else run_prestart(numdsnodes=int(sys.argv[2]))
		elif(command == 'prestartguard'):
			print_usage() if (numargs != 3) else run_prestart(numdsnodes=int(sys.argv[2]), guard_mode=True)
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
		elif (command == 'startpow'):
			print_usage() if (numargs != 9) else run_startpow(nodenum=int(sys.argv[2]), dscount=int(sys.argv[3]), blocknum=sys.argv[4], dsdiff=sys.argv[5], diff=sys.argv[6], rand1=sys.argv[7], rand2=sys.argv[8])
		elif (command == 'createtx'):
			print_usage() if (numargs != 6) else run_createtx(nodenum=int(sys.argv[2]), fromnode=int(sys.argv[3]), tonode=int(sys.argv[4]), amount=int(sys.argv[5]))
		elif (command == 'delete'):
			print_usage() if (numargs != 2) else run_delete()
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
		shutil.copyfile('./tests/Zilliqa/zilliqa', testsubdir + '/zilliqa')
		shutil.copyfile('./tests/Zilliqa/sendcmd', testsubdir + '/sendcmd')

		st = os.stat(testsubdir + '/zilliqa')
		os.chmod(testsubdir + '/zilliqa', st.st_mode | stat.S_IEXEC)
		st = os.stat(testsubdir + '/sendcmd')
		os.chmod(testsubdir + '/sendcmd', st.st_mode | stat.S_IEXEC)

	if printnodes:
		testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
		count = len(testfolders_list)
		for x in range(0, count):
			print '[Node ' + str(x + 1).ljust(3) + '] [Port ' + str(NODE_LISTEN_PORT + x) + '] ' + LOCAL_RUN_FOLDER + testfolders_list[x]


def run_prestart(numdsnodes, guard_mode=False):
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)
	keypairs = []
	
	range_begin_index = 0



	if guard_mode == True:
		keypairs = ["020F325C2EBA2EA8EDED2E575F1FFBE581ACCA4064CB5BD4666FF15CCCBA6A7093 A6826C4D72DD53971DFAEBB591733AC63B4DFEEBD11383FE5C4CF57061C2A615",
					"02128D7C7D6D2AF2503648DEA76AFC9EACDC30748A5D22489142E7666ECB1D9B4E 9C66312E390005EDFF13D0CA6D2082DC0C18123B3FB777ED0CF61690F14BD07F",
					"02145C485DC33D903EBDBAD1D6781250B958816F414232683FD5BF2348A69EF741 B4E480E8744F4CEB975E5E329C9459CD15F7565DD7C6F9DF649001AA2D974996",
					"021D024623D979C6C2873E3FEA26F9F59DB91330E250511DACBC6E85F3A2EE0927 F6132FCD7A150C4B327EE43787B25A6C247B9B3441F8350F28F80BF8D643D785",
					"022396E4F41AD7494EC410EFFC5C0F9AFB707CEE9B62047BED0A5E614355ED67EC 8337B8644DB1BD151254A5CE82EA151622F22FDE7F84FF544203FA194D837387"]
		range_begin_index = len(keypairs)

	# Generate keypairs
	for x in range(range_begin_index, count):
		process = Popen(["./tests/Zilliqa/genkeypair"], stdout=PIPE)
		(output, err) = process.communicate()
		exit_code = process.wait()
		keypairs.append(output)

	if guard_mode == True:
		keypairs[10] = "03AFF08ECE9CA58665D412C0F89245D029A64F1794EDE9A9BA4262B00C5BA7E1D1 4B8C63E658B51604790BD8383E56A317201EAF71074A933DC31132E5DB0D7FC3"
		keypairs[11] = "03BB22048026A6CD33F670376BE14DDE7D44A9B0B5F1EF8427AC717E18B15A29EF 508AFA26888DD979762187B34D17ADC861E0F422FC5616D72824A9BC5FCC9278"
		keypairs[12] = "03BF863614661B940278BCC582CCCF33FCD19501CDE2CFAA9A3F4061592F736569 CEA5827E750E8650AA5FC277E95BF9CA02D2B8DFA71481675AA1ABFD2928A61C"
		keypairs[13] = "03DCAB61B736BDC1E77129B4363A76FF74CE89EC49A377A3DCDFE7AE45EF12C3F8 E8A72190FB00E709A216E92E435C65FC508180B309A70D0982A2EC27B794945A"
		keypairs[14] = "03DE502D5A7F6277EEA30BBD58120E4BBD091EF87912D834225104C57E9B39A918 C4EE5BC694EE6EC0488E42B462DA01BA4B254B6A2F6CE83895C018B74EDF49F6"

	nodes = ET.Element("nodes")
	dsnodes = ET.Element("dsnodes");

	# Store sorted keys list in text file
	keys_file = open(LOCAL_RUN_FOLDER + 'keys.txt', "w")
	for x in range(0, count):
		keys_file.write(keypairs[x] + '\n')
		keypair = keypairs[x].split(" ")
		
		if (x < numdsnodes):
			ET.SubElement(dsnodes, "pubk").text = keypair[0];
			peer = ET.SubElement(nodes, "peer")
			ET.SubElement(peer, "pubk").text = keypair[0]
			ET.SubElement(peer, "ip").text = '127.0.0.1'
			ET.SubElement(peer, "port").text = str(NODE_LISTEN_PORT + x)
	keys_file.close()

	#Create dsnodes file
	dsTree = ET.ElementTree(dsnodes)
	dsTree.write("dsnodes.xml")
	dsnodes.clear()

	# Create config_normal.xml with pubkey and IP info of all DS nodes
	tree = ET.ElementTree(nodes)
	tree.write("config_normal.xml")

	# Clear the element tree 
	nodes.clear()

	# ds_whitelist.xml generation
	keys_file = open(LOCAL_RUN_FOLDER + 'keys.txt', "w")
	for x in range(0, count):
		keys_file.write(keypairs[x] + '\n')
		keypair = keypairs[x].split(" ")
		peer = ET.SubElement(nodes, "peer")
		ET.SubElement(peer, "pubk").text = keypair[0]
		ET.SubElement(peer, "ip").text = '127.0.0.1'
		ET.SubElement(peer, "port").text = str(NODE_LISTEN_PORT + x)
	keys_file.close()

	# Create ds_whitelist.xml with pubkey and IP info of all DS nodes
	tree = ET.ElementTree(nodes)
	tree.write("ds_whitelist.xml")

	# clear from ds_whitelist
	nodes.clear()

	address_nodes = ET.Element("address")
	# shard_whitelist.xml generation
	keys_file = open(LOCAL_RUN_FOLDER + 'keys.txt', "w")
	for x in range(0, count):
		keys_file.write(keypairs[x] + '\n')
		keypair = keypairs[x].split(" ")
		ET.SubElement(address_nodes, "pubk").text = keypair[0]
	keys_file.close()

	# Create shard_whitelist.xml with pubkey
	tree = ET.ElementTree(address_nodes)
	tree.write("shard_whitelist.xml")

def run_start(numdsnodes):

	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)

	# Load the keypairs
	keypairs = []
	with open(LOCAL_RUN_FOLDER + 'keys.txt') as f:
		keypairs = f.readlines()
	keypairs = [x.strip() for x in keypairs]

	# Launch node Zilliqa process
	for x in range(0, count):
		keypair = keypairs[x].split(" ")
		shutil.copyfile('ds_whitelist.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/ds_whitelist.xml')
		shutil.copyfile('shard_whitelist.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/shard_whitelist.xml')
		shutil.copyfile('constants_local.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml')
		shutil.copyfile('dsnodes.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/dsnodes.xml')

		if (x < numdsnodes):
			shutil.copyfile('config_normal.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/config.xml')
			os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; $(pwd)/zilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + '127.0.0.1' +' ' + str(NODE_LISTEN_PORT + x) + ' 1 0 0 > ./error_log_zilliqa 2>&1 &')
		else:
			os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; $(pwd)/zilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + '127.0.0.1' +' ' + str(NODE_LISTEN_PORT + x) + ' 0 0 0 > ./error_log_zilliqa 2>&1 &')

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
	os.system('tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT + nodenum - 1) + ' cmd ' + msg)
	
def run_sendcmdrandom(nodenum, msg_size):
	# msg = "000400" + 'A' * msg_size * 2
	# os.system('tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT + nodenum - 1) + ' cmd ' + msg)
	os.system('tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT) + ' broadcast ' + msg_size)

def run_startpow(nodenum, dscount, blocknum, dsdiff, diff, rand1, rand2):
	testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
	count = len(testfolders_list)

	# Load the keypairs
	keypairs = []
	with open(LOCAL_RUN_FOLDER + 'keys.txt') as f:
		keypairs = f.readlines()
	keypairs = [x.strip() for x in keypairs]

	# Assemble the STARTPOW message
	startpow_cmd = 'tests/Zilliqa/sendcmd ' + str(NODE_LISTEN_PORT + nodenum - 1) + ' cmd 0200' + blocknum + dsdiff + diff + rand1 + rand2
	for x in range(0, dscount):
		keypair = keypairs[x].split(" ")
		startpow_cmd = startpow_cmd + keypair[0] + '0000000000000000000000000100007F' + "{0:0{1}x}".format(NODE_LISTEN_PORT + x, 8)

	# Send to node
	os.system(startpow_cmd)

def run_delete():
	if (os.path.exists(LOCAL_RUN_FOLDER)):
		shutil.rmtree(LOCAL_RUN_FOLDER)

if __name__ == "__main__":
	main()
