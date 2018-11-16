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
import socket, errno
import signal

from subprocess import Popen, PIPE

IP_SITE = 'ifconfig.me'

PORT_NUM = 30303
PROJ_DIR = '/run/zilliqa'

def main():
	if len(sys.argv) == 6:
		run_restart(sys.argv[1],sys.argv[2],sys.argv[3], sys.argv[4], sys.argv[5] + '/')
	elif len(sys.argv) == 5:
		run_restart(sys.argv[1],sys.argv[2],sys.argv[3], sys.argv[4], '')
	else:
		print "Not enough args"

def get_immediate_subdirectories(a_dir):
	subdirs = [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]
	subdirs.sort()
	return subdirs

def getIP():
	return socket.gethostbyname(socket.gethostname())

def isPortOpen(port):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	
	try:
		sock.bind(("127.0.0.1", port))

	except socket.error as e:
		if e.errno == errno.EADDRINUSE:
			return False

	sock.close()
	return True


def run_restart(pubKey, privKey, port, typ, path):

		
	keypairs = []

	keypairs.append(privKey + " " + pubKey)

	nodeIP = getIP()

	while(not isPortOpen(int(port))):
		time.sleep(2)


	

	for x in range(0, 1):
		keypair = keypairs[x].split(" ")

		signal.signal(signal.SIGCHLD, signal.SIG_IGN)
		os.system('cd ' + PROJ_DIR + '; ulimit -Sc unlimited; ulimit -Hc unlimited;' + path + 'zilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + nodeIP +' ' + str(PORT_NUM) + ' 1 '+typ+ ' 1 >> ./error_log_zilliqa 2>&1')

if __name__ == "__main__":
	main()
