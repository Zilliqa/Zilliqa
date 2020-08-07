#!/usr/bin/env python3
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

import json
import requests
import os, time
import hashlib
import sys
import argparse
import csv
import socket

DEBUG_MODE = False

def generate_payload(params, methodName, id = 1):

	if params and not type(params) is list:
		params = [params]
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
		part = sock.recv(BUFF_SIZE)
		data += part.decode()
		if len(part) < BUFF_SIZE:
			# either 0 or end of data
			break
	return data

def gen_payload_batch(params, methodName):
	req = []
	count = 1
	for i in params:
		data = generate_payload(i, methodName, count)
		req.append(data)
		count = count + 1
	return req

def send_packet_tcp(data, host, port):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	try:
		sock.connect((host, port))
		data = data + "\n"
		sock.sendall(data.encode())
		received = recvall(sock)
	except socket.error:
		print("Socket error")
		sock.close()
		return None
	sock.close()
	return received

def get_response(params, methodName, host, port, batch):
	
	if not batch:
		data = json.dumps(generate_payload(params, methodName))
	else:
		data = json.dumps(gen_payload_batch(params, methodName))
	if DEBUG_MODE:
		print("Request:\n\t"+data)
	recv = send_packet_tcp(data, host, port)
	if not recv:
		return None
	response = json.loads(recv)

	if DEBUG_MODE:
		print("Response:\n\t"+recv)
	return response

def parse_arguments(options):
	parser = argparse.ArgumentParser()
	parser.add_argument("--address","-a",help="host address for querying, default: localhost", default="127.0.0.1")
	parser.add_argument("option",help="input option for the query", choices=options)
	parser.add_argument("--port", "-p", help="port to query",default =4301,type=int )
	parser.add_argument("--params","-pm",help="parameters", nargs='+')
	parser.add_argument("--debug","-d",help="debug mode", action='store_true')

	args = parser.parse_args()
	return args

def make_options_dictionary(options_dict):
	options_dict["epoch"] = "GetCurrentMiniEpoch"
	options_dict["dsepoch"] = "GetCurrentDSEpoch"
	options_dict["type"] = "GetNodeType"
	options_dict["ds"] = "GetDSCommittee"
	options_dict["state"] = "GetNodeState"
	options_dict["checktxn"] = "IsTxnInMemPool"
	options_dict["whitelist_add"] = "AddToBlacklistExclusion"
	options_dict["whitelist_remove"] = "RemoveFromBlacklistExclusion"
	options_dict["register_extseed"] = "AddToExtSeedWhitelist"
	options_dict["deregister_extseed"] = "RemoveFromExtSeedWhitelist"
	options_dict["reglist_extseed"] = "GetWhitelistedExtSeed"
	options_dict["seedswhitelist_add"] = "AddToSeedsWhitelist"
	options_dict["seedswhitelist_remove"] = "RemoveFromSeedsWhitelist"
	options_dict["ds_difficulty"] = "GetPrevDSDifficulty"
	options_dict["difficulty"] = "GetPrevDifficulty"
	options_dict["set_sendsccallstods"] = "ToggleSendSCCallsToDS"
	options_dict["get_sendsccallstods"] = "GetSendSCCallsToDS"
	options_dict["disable_pow"] = "DisablePoW"
	options_dict["disabletxns"] = "ToggleDisableTxns"
	options_dict["set_validatedb"] = "SetValidateDB"
	options_dict["get_validatedb"] = "GetValidateDB"

def ProcessResponseCore(resp, param):
	if param:
		print("Parameter: "+str(param)+"\t:")
	try:
		print(resp["result"])
	except KeyError:
		print(resp["error"]	)

def ProcessResponse(resp, params, batch):
	if not batch:
		ProcessResponseCore(resp, params)
	else:
		#Assuming the params are in same order as thier starting from 1,2..
		for i in resp:
			try:
				param = params[int(i["id"])-1]
				ProcessResponseCore(i, param)
			except KeyError:
				print("Could not find id: "+i["id"])

def main():
	options_dictionary = {}
	make_options_dictionary(options_dictionary)
	option_param_required = ["checktxn","whitelist_add","whitelist_remove","seedswhitelist_add", "seedswhitelist_remove", "register_extseed", "deregister_extseed"]
	global DEBUG_MODE
	args = parse_arguments(sorted(options_dictionary.keys()))
	DEBUG_MODE = args.debug
	batch = False

	if args.option in option_param_required:
		if len(args.params) == 0:
			print("Error: Params cannot be empty")
			return 1
		batch = True

	response = get_response(args.params, options_dictionary[args.option], args.address, args.port, batch)
	
	if response == None:
		print("Could not get result")
	else:
		ProcessResponse(response,args.params, batch)

if __name__ == '__main__':
	main()