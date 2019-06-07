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

import argparse
import datetime
import json
import os.path
import re
import sys
import time
from urllib import request, parse

from pprint import pprint
from pyzil.crypto import zilkey
from pyzil.zilliqa import chain
from pyzil.zilliqa.units import Zil, Qa
from pyzil.account import Account, BatchTransfer
from pyzil.zilliqa.api import ZilliqaAPI, APIError

SEND_ZILS_FORWARD_OR_BACK = True

def send_report(msg, url):
	post = {'text': '```' + msg + '```'}
	json_data = json.dumps(post)
	req = request.Request(url, data=json_data.encode('ascii'))
	resp = request.urlopen(req)

def test_txn(args):
	global ERROR_MESSAGE
	global SEND_ZILS_FORWARD_OR_BACK

	# Load source account
	account = Account(address=args['srchex'], private_key=args['srckey'])
	balance = account.get_balance()

	# Load destination account
	account2 = Account(address=args['dsthex'], private_key=args['dstkey'])
	balance2 = account2.get_balance()

	if SEND_ZILS_FORWARD_OR_BACK == True:
		print("SRC: {}: {}".format(account, balance))
		print("DST: {}: {}".format(account2, balance2))
		# Send 100 Qa from srchex to dstzil
		txn_info = account.transfer(to_addr=args['dstzil'], zils=Qa(100))
		SEND_ZILS_FORWARD_OR_BACK = False
	else:
		print("SRC: {}: {}".format(account2, balance2))
		print("DST: {}: {}".format(account, balance))
		# Send 100 Qa from dsthex to srczil
		txn_info = account2.transfer(to_addr=args['srczil'], zils=Qa(100))
		SEND_ZILS_FORWARD_OR_BACK = True

	pprint(txn_info)
	txn_id = txn_info["TranID"]

	sys.stdout.flush()

	# Wait for confirmation (timeout = 20mins, to take into account a view change)
	txn_details = account.wait_txn_confirm(txn_id, timeout=1200)
	pprint(txn_details)
	if txn_details and txn_details["receipt"]["success"]:
	    print("Txn success: {}".format(txn_id))
	else:
	    print("Txn failed: {}".format(txn_id))
	    raise Exception("Txn failed: {}".format(txn_id))

def parse_arguments():
	parser = argparse.ArgumentParser(description='Script to check if testnet can still process txns (NOTE: This only works for CHAIN_ID=1)')

	parser.add_argument("--srchex", help="Src address (base16, omit 0x)", required=True)
	parser.add_argument("--srczil", help="Src address (bech32)", required=True)
	parser.add_argument("--srckey", help="Src privkey (omit 0x)", required=True)

	parser.add_argument("--dsthex", help="Dst address (base16, omit 0x)", required=True)
	parser.add_argument("--dstzil", help="Dst address (bech32)", required=True)
	parser.add_argument("--dstkey", help="Dst privkey (omit 0x)", required=True)

	parser.add_argument("--frequency", help="Checking frequency in minutes (default = 0 or run once)", required=False, default=0)
	parser.add_argument("--apiurl", help="URL for querying", required=True)
	parser.add_argument("--webhook", help="Slack webhook URL", required=False, default='')

	args = vars(parser.parse_args())
	return args

def main():
	args = parse_arguments()
	pprint(args)

	frequency = int(args['frequency']) * 60

	# Set active chain
	chain.set_active_chain(chain.BlockChain(args['apiurl'], version=65537, network_id=1))

	while True:
		print('Check started at: ' + str(datetime.datetime.now()))

		global ERROR_MESSAGE
		ERROR_MESSAGE = ''

		try:
			test_txn(args)
		except Exception as e:
			ERROR_MESSAGE = '[' + os.path.basename(__file__) + '] Error: ' + str(e)

		if (ERROR_MESSAGE != ''):
			print(ERROR_MESSAGE)
			if args['webhook'] != '':
				send_report(ERROR_MESSAGE, args['webhook'])

		sys.stdout.flush()

		if frequency == 0:
			break

		time.sleep(frequency)

if __name__ == '__main__':
	main()