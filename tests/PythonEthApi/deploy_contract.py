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

import sys
import argparse

import solcx
solcx.install_solc()

import eth_account.signers.local
import pyzil.account
import requests
import os
import time

import web3
from web3 import Web3
from web3._utils.abi import get_constructor_abi, get_abi_output_types
from web3._utils.contracts import encode_abi, get_function_info

from pyzil.zilliqa.chain import BlockChain, set_active_chain, active_chain, ZilliqaAPI
from pyzil.account import Account
from pyzil.crypto.zilkey import to_checksum_address

w3 = Web3()

FILE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BLOCK_TIME_S = 2

GAS_LIMIT = 250000
GAS_PRICE = 60
CHAIN_ID = 33101

contract = """
// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

/**
 * @title Storage
 * @dev Store & retrieve value in a variable
 */
contract Storage {

    uint256 number = 1234;
    uint256 numberSecond = 1025;
    /**
     * @dev Store value in variable
     * @param num value to store
     */
    function store(uint256 num) public {
        number = num;
    }

    /**
     * @dev Return value
     * @return value of 'number'
     */
    function retrieve() public view returns (uint256){
        return number;
    }

   function wtf() public {
        selfdestruct(payable(address(0)));
    }
}
"""

def get_result(response: requests.models.Response) -> any:
    if response.status_code != 200:
        raise Exception(f"Bad status code {response.status_code} - {response.text}")

    res = response.json()

    if "result" not in res.keys():
        raise Exception(f"Bad JSON, no result found: {response.text}")

    return res["result"]

def compile_solidity(contract_string):
    result = solcx.compile_source(
        contract_string, output_values=["abi", "bin", "asm"]
    )
    result = result.popitem()[1]
    return w3.eth.contract(abi=result["abi"], bytecode=result["bin"])


def parse_commandline():
    parser = argparse.ArgumentParser()
    parser.add_argument('--api', type=str, required=True, help='API to test against')
    parser.add_argument('--private-key-genesis', type=str, help='Private key found in genesis',
                        default="db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3")
    parser.add_argument('--private-key-test', type=str, help='Private key to move genesis funds to, for test usage',
                        default="a8b68f4800bc7513fca14a752324e41b2fa0a7c06e80603aac9e5961e757d906")

    return parser.parse_args()

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

        # Note the address needs to be ZIL style checksum or the pyzil api will reject it (and possibly the node too)
        genesis_privkey.transfer(to_addr=to_address, zils=to_move, confirm=True, gas_limit=50)

        # Now check balance of eth address (annoyingly, does not accept '0x')
        newBal = api.GetBalance(to_checksum_address(to_address, prefix=""))

        if "balance" not in newBal:
            raise Exception(f"Bad response from balance query: {newBal}")

        newBal = newBal["balance"]

        if bal == 0:
            raise Exception(f"Failed to see funds for eth style test address. Subsequent tests will likely fail.")
    except Exception as e:
        print(f"Failed test test_move_funds with error: '{e}'")
        return False

    return True

def test_eth_getStorageAt(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the value from a storage position at a given address, or in other words,
        returns the state of the contract's storage, which may not be exposed
        via the contract's methods
    """
    try:
        # In order to do this test, we can write a contract to storage and check its state is there correctly
        nonce = w3.eth.getTransactionCount(account.address)

        compilation_result = compile_solidity(contract)
        code = compilation_result.constructor().data_in_transaction

        transaction = {
            'to': "0x0000000000000000000000000000000000000000",
            'from':account.address,
            'value':int(0),
            'data':code,
            'gas':GAS_LIMIT,
            'gasPrice':int(GAS_PRICE*(10**9)),
            'chainId':CHAIN_ID,
            'nonce':int(nonce)
        }

        signed_transaction = account.signTransaction(transaction)
        rawHex = signed_transaction.rawTransaction.hex()

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sendRawTransaction",
                                            "params": [rawHex]})
        res = get_result(response)

        # Get the address of the contract (ZIL API, TODO: CHANGE THIS)
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "GetContractAddressFromTransactionID",
                                            "params": [res]})

        res = get_result(response)
        res.replace("0x", "")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getStorageAt",
                                            "params": [res, "0x1", "latest"]})

        res = get_result(response)

        if "0x0000000000000000000000000000000000000000000000000000000000000401" != res.lower():
            raise Exception(f"Failed to retrieve correct contract amount: {res} when expected 0x0000000000000000000000000000000000000000000000000000000000000401")

    except Exception as e:
        print(f"Failed test test_eth_getStorageAt with error: '{e}'")
        return False

    return True

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

    ret = True

    ret &= test_move_funds(args.api, genesis_privkey, account, api)
    ret &= test_eth_getStorageAt(args.api, account, w3)

    if not ret:
        print(f"Test failed")
        sys.exit(1)
    else:
        print(f"Test passed!")


if __name__ == '__main__':
    main()
