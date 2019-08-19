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
import socket, errno
import signal

from subprocess import Popen, PIPE

IP_SITE = 'ifconfig.me'
PROJ_DIR = '/run/zilliqa'
#PROJ_DIR = '/home/shengguang/Test/Zilliqa/build/bin' # For local test

def main():
	if len(sys.argv) == 8:
		run_restart(sys.argv[1],sys.argv[2],sys.argv[3], sys.argv[4], sys.argv[5], '', sys.argv[6], sys.argv[7])
	elif len(sys.argv) == 7:
		run_restart(sys.argv[1],sys.argv[2],sys.argv[3], sys.argv[4], sys.argv[5], '', './', sys.argv[6])
	else:
		print "Not enough args"

def get_immediate_subdirectories(a_dir):
	subdirs = [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]
	subdirs.sort()
	return subdirs

def isPortOpen(port):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	
	try:
		sock.bind(("127.0.0.1", port))

	except socket.error as e:
		if e.errno == errno.EADDRINUSE:
			return False

	sock.close()
	return True

def run_restart(pubKey, privKey, ip, port, typ, path, logPath, recovery):
	keypairs = []
	keypairs.append(privKey + " " + pubKey)

	while(not isPortOpen(int(port))):
		time.sleep(2)

	for x in range(0, 1):
		keypair = keypairs[x].split(" ")
		signal.signal(signal.SIGCHLD, signal.SIG_IGN)
		zilliqaCmd = path + 'zilliqa' + ' --privk ' + keypair[0] + ' --pubk ' + keypair[1] + ' --address ' + ip + ' --port ' + str(port) + ' --synctype ' + typ + ' --logpath ' + logPath
		if recovery is not 0:
			zilliqaCmd += ' --recovery'
		os.system('cd ' + PROJ_DIR + '; ulimit -Sc unlimited; ulimit -Hc unlimited;' + zilliqaCmd + ' >> ./error_log_zilliqa 2>&1')

if __name__ == "__main__":
	main()
