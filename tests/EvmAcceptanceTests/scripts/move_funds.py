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

import argparse
import sys
import traceback

import solcx

solcx.install_solc()

import os
import time

import eth_account.signers.local
import pyzil.account
import requests
import web3
from pyzil.account import Account
from pyzil.crypto.zilkey import to_checksum_address
from pyzil.zilliqa.chain import (BlockChain, ZilliqaAPI, active_chain,
                                 set_active_chain)
from web3 import Web3
from web3._utils.abi import get_abi_output_types, get_constructor_abi
from web3._utils.contracts import encode_abi, get_function_info

w3 = Web3()

def test_move_funds(url: str, genesis_privkey: pyzil.account, test_privkey: eth_account.signers.local.LocalAccount, api: ZilliqaAPI) -> bool:
    """
        Test the genesis funds are there, and can be moved to the new key (using eth style addressing)
    """
    try:
        # Check genesis has funds using pyzil api
        bal = genesis_privkey.get_balance()

        print(f"Balance is {bal}")
        print(f"Account is {genesis_privkey.address}")

        if bal == 0:
            raise Exception(f"Genesis provided balance is 0! Most tests will fail.")

        # Move half of these funds to the address that will be used for testing
        to_move = bal / 2
        to_address = test_privkey.address
        to_address = to_checksum_address(to_address, prefix="0x")
        to_address_eth_crc = Web3.toChecksumAddress(to_address.lower())

        print(f"To (eth) addr is {to_address}")

        # Note the address needs to be ZIL style checksum or the pyzil api will reject it (and possibly the node too)
        genesis_privkey.transfer(to_addr=to_address, zils=to_move, confirm=True, gas_limit=50)

        # Now check balance of eth address (annoyingly, does not accept '0x')
        newBal = api.GetBalance(to_checksum_address(to_address, prefix=""))

        print(f"new balance is {newBal}")

        if "balance" not in newBal:
            raise Exception(f"Bad response from balance query: {newBal}")

        newBal = newBal["balance"]

        if bal == 0:
            raise Exception(f"Failed to see funds for eth style test address. Subsequent tests will likely fail.")
    except Exception as e:
        print(f"********* Failed test test_move_funds with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True


def parse_commandline():
    parser = argparse.ArgumentParser()
    parser.add_argument('--api', type=str, required=True, help='API to test against')
    parser.add_argument('--private-key-genesis', type=str, help='Private key found in genesis',
                        default="db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3")
    parser.add_argument('--private-key-test', type=str, help='Private key to move genesis funds to, for test usage',
                        default="db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3")

    return parser.parse_args()

def main():
    args = parse_commandline()

    # Force API to end with slash
    if args.api[-1] != '/':
        args.api += '/'

    print(f"args are {args.api}")

    # Set up ZIL api
    api = ZilliqaAPI(args.api)
    blockchain = BlockChain(api_url=args.api, version=65537, network_id=1)
    set_active_chain(blockchain)

    print(f"Moving funds to private addr.")

    genesis_privkey = Account(private_key=args.private_key_genesis) # Zil style

    # Set up w3 API
    w3 = Web3(Web3.HTTPProvider(args.api))

    account = web3.eth.Account.from_key(args.private_key_test)

    test_move_funds(args.api, genesis_privkey, account, api)
    print("Finished!")


if __name__ == '__main__':
    main()
