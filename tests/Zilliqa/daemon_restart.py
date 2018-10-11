import subprocess
import os
import sys
import shutil
import stat
import time
import socket, errno

from subprocess import Popen, PIPE

IP_SITE = 'ifconfig.me'

PORT_NUM = 30303
PROJ_DIR = 'zilliqa-test'

def main():
	if len(sys.argv) == 6:
		run_restart(sys.argv[1],sys.argv[2],sys.argv[3], sys.argv[4], sys.argv[5])
	else:
		print "Not enough args"

def get_immediate_subdirectories(a_dir):
	subdirs = [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]
	subdirs.sort()
	return subdirs

def getIP():
	process = Popen(["curl",IP_SITE], stdout = PIPE )
	(output, err) = process.communicate()
	exit_code = process.wait()
	return output.strip('\n')

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

		os.system('cd ~/' + PROJ_DIR + '; ulimit -Sc unlimited; ulimit -Hc unlimited;' + path + '/zilliqa ' + keypair[1] + ' ' + keypair[0] + ' ' + nodeIP +' ' + str(PORT_NUM) + ' 0 '+typ+ ' 1 >> ./error_log_zilliqa 2>&1 &')


if __name__ == "__main__":
	main()
