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

def generate_payload(params, methodName, id = 1):
	payload = {"method":methodName,
		"params":params,
		"jsonrpc":"2.0",
		"id" : id
	}
	return payload


def get_response(params, methodName, url, id = 1):
	headers = {'content-type' : 'application/json'}
	#print url
	payload = generate_payload(params, methodName)
	response = requests.post(url, data = json.dumps(payload), headers = headers)

	if response.status_code != requests.codes.ok:
		print "Error fetching "+ str(requests.status_codes._codes[response.status_code][0])
		return None

	assert (response.json()["id"] == id)
	try:
		return response.json()["result"]
	except KeyError:
		print response.json()["error"]
		return None



def parse_arguments(options):
	parser = argparse.ArgumentParser()
	parser.add_argument("--address","-a",help="URL for querying, default: localhost", default="127.0.0.1")
	parser.add_argument("option",help="input option for the query", choices=options)
	args = parser.parse_args()
	return args

def make_options_dictionary(options_dict):
	options_dict["epoch"] = "GetCurrentMiniEpoch"
	options_dict["dsepoch"] = "GetCurrentDSEpoch"

def main():
	options_dictionary = {}
	make_options_dictionary(options_dictionary)
	PORT = 4201; #default port	

	args = parse_arguments(options_dictionary.keys())
	
	query_url = "http://"+args.address+":"+str(PORT);

	response = get_response([],options_dictionary[args.option],query_url)

	if response == None:
		print "Could not get result"
	else:
		print response

	

if __name__ == '__main__':
	main()

	

