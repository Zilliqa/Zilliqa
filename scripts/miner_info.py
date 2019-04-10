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

#!/usr/bin/python
import json
import requests
import os, time
import hashlib
import sys
import argparse
import csv
import socket

def generate_payload(params, methodName, id = 1):
	payload = {"method":methodName,
		"params":params,
		"jsonrpc":"2.0",
		"id" : id
	}
	return payload

def recvall(sock):
	BUFF_SIZE = 4096 # 4 KiB
	data = ""
	while True:
		part = str(sock.recv(BUFF_SIZE))
		data += part
		if len(part) < BUFF_SIZE:
			# either 0 or end of data
			break
	return data


def get_response(params, methodName, host, port, id = 1):

	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	try:
		sock.connect((host, port))
		data = json.dumps(generate_payload(params, methodName))
		sock.sendall(bytes(data+"\n"))      
		received = recvall(sock)
	except socket.error:
		print "Socket error "
		sock.close()
		return None

	response = json.loads(received)

	sock.close()

	assert (response["id"] == id)
	try:
		return response["result"]
	except KeyError:
		print response["error"]
		return None



def parse_arguments(options):
	parser = argparse.ArgumentParser()
	parser.add_argument("--address","-a",help="host address for querying, default: localhost", default="127.0.0.1")
	parser.add_argument("option",help="input option for the query", choices=options)
	parser.add_argument("--port", "-p", help="port to query",default =4201,type=int )
	parser.add_argument("--params","-pm", nargs='?', help="parameter for the request")
	args = parser.parse_args()
	return args

def make_options_dictionary(options_dict):
	options_dict["epoch"] = "GetCurrentMiniEpoch"
	options_dict["dsepoch"] = "GetCurrentDSEpoch"
	options_dict["type"] = "GetNodeType"
	options_dict["ds"] = "GetDSCommittee"
	options_dict["state"] = "GetNodeState"
	options_dict["checktxn"] = "IsTxnInMemPool"

def main():
	options_dictionary = {}
	make_options_dictionary(options_dictionary)

	args = parse_arguments(options_dictionary.keys())

	if args.option == "checktxn" and not args.params:
		print "\033[91mError\033[0m: Please use '--params' to pass valid txnhash for checktxn"
		return 0

	response = get_response(args.params,options_dictionary[args.option],args.address, args.port)

	if response == None:
		print "Could not get result"
	else:
		print response

	

if __name__ == '__main__':
	main()

	

