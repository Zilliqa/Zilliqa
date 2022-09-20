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
CHAIN_ID = 32769 # 33101

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

def install_contract_bytes(account, data_bytes):
    txn_details = account.transfer(
        to_addr="0x0000000000000000000000000000000000000000",
        zils=0,
        code=data_bytes.replace("0x", "EVM"),
        gas_limit=99_000,
        gas_price=2000000000,
        priority=True,
        data="",  # TODO: Change for constructor params.
        confirm=True,
    )
    if "ID" in txn_details:
        address = Web3.toChecksumAddress(
            active_chain.api.GetContractAddressFromTransactionID(txn_details["ID"])
        )
        print("Contract created, address: {}".format(address))
        return w3.eth.contract(address=address)
    else:
        raise "No ID in the contract"

def call_contract(account, contract, value, method, *arguments):
    """
    Call the contract's method with arguments as transaction.
    """
    # Use contract ABI to encode arguments.
    calldata = contract.encodeABI(fn_name=method, args=arguments).replace("0x", "")
    print("Calldata:", calldata)
    contract_addr = to_checksum_address(contract.address)
    print("Contract addr:", contract_addr)
    txn_details = account.transfer(
        to_addr=contract_addr,
        zils=value,
        gas_limit=99_000,
        gas_price=2000000000,
        data=calldata,
        confirm=True,
    )
    return txn_details

def test_eth_feeHistory(url: str) -> bool:
    """
      Returns a collection of historical gas information from which you can decide what
       to submit as your maxFeePerGas and/or maxPriorityFeePerGas. This method
        was introduced with EIP-1559. https://docs.alchemy.com/alchemy/apis/ethereum/eth-feehistory
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_feeHistory", "params": [4, "latest", [25, 75]] })
        get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_feeHistory with error: '{e}'")
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

def test_eth_getCode(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Get a code at the specified address. Which is a smart contract address.
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

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getCode",
                                            "params": [res, "latest"]})

        res = get_result(response)

        if code[0:4] != res[0:4].lower():
            raise Exception(f"Failed to retrieve correct contract amount: {res} when expected {code}")

    except Exception as e:
        print(f"Failed test test_eth_getCode with error: '{e}'")
        return False

    return True

def test_eth_getProof(url: str)   -> bool:
    """
        Returns the account and storage values of the specified account, including the Merkle proof.
        The API allows IoT devices or mobile apps which are unable to run light clients to verify
        responses from untrusted sources, by using a trusted block hash.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getProof", "params":["0xb59f67a8bff5d8cd03f6ac17265c550ed8f33907", "latest"]})
        get_result(response)

    except Exception as e:
        print(f"Failed test test_eth_getProof with error: '{e}'")
        return False

    return True

def test_eth_getBalance(url: str) -> bool:
    """
        Returns the balance of the account of a given address.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBalance", "params":["0xc94770007dda54cF92009BFF0dE90c06F603a09f", "latest"]})
        get_result(response)

    except Exception as e:
        print(f"Failed test test_eth_getBalance with error: '{e}'")
        return False

    return True

def test_web3_clientVersion(url: str) -> bool:
    """
        Returns the current client version.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_web3_clientVersion"})
        res = get_result(response)

        if "0x66" not in res:
            raise Exception(f"Bad client version: {res}")

    except Exception as e:
        print(f"Failed test eth_web3_clientVersion with error: '{e}'")
        return False

    return True

def test_web3_sha3(url: str) -> bool:
    """
        Returns the string provided as a sha3.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_web3_sha3", "params":["0x68656c6c6f20776f726c64"]})
        res = get_result(response)

        if "0x47173285a8d7341e5e972fc677286384f802f8ef42a5ec5f03bbfa254cb01fad" not in res.lower():
            raise Exception(f"Bad sha3 return value: {res}")

    except Exception as e:
        print(f"Failed test test_web3_sha3 with error: '{e}'")
        return False

    return True

def test_net_version(url: str) -> bool:
    """
        net_version should return a network id like Ethereum which is a number that looks like it is predefined.
        Some sites show it as deprecated and some tell you should use chain_id. To be save we can decide
        to make it unsupported.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "net_version"})
        res = get_result(response)

        if "" not in res.lower():
            raise Exception(f"Bad net_version return value: {res}")

    except Exception as e:
        print(f"Failed test test_net_version with error: '{e}'")
        return False

    return True

def test_net_listening(url: str) -> bool:
    """
        Returns true if client is actively listening for network connections. Always false
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "net_listening"})
        res = get_result(response)

        if res is not False:
            raise Exception(f"Bad net_listening return value: {res}")

    except Exception as e:
        print(f"Failed test test_net_listening with error: '{e}'")
        return False

    return True

def test_net_peerCount(url: str) -> bool:
    """
        will always return 0x0
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "net_peerCount"})
        res = get_result(response)

        if "0x0" not in res.lower():
            raise Exception(f"Bad net_listening return value: {res}")

    except Exception as e:
        print(f"Failed test test_net_peerCount with error: '{e}'")
        return False

    return True

def test_eth_protocolVersion(url: str) -> bool:
    """
        will always return ''
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_protocolVersion"})
        res = get_result(response)

        if res != "":
            raise Exception(f"Bad eth_protocolVersion return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_protocolVersion with error: '{e}'")
        return False

    return True

def test_eth_syncing(url: str) -> bool:
    """
        will always return false
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_syncing"})
        res = get_result(response)

        if res is not False:
            raise Exception(f"Bad eth_syncing return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_syncing with error: '{e}'")
        return False

    return True

def test_eth_coinbase(url: str) -> bool:
    """
        Returns the client coinbase address. The coinbase address is the account to pay mining rewards to.
        return all 0s
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_coinbase"})
        res = get_result(response)

        if res.lower() != "0x0000000000000000000000000000000000000000":
            raise Exception(f"Bad eth_coinbase return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_coinbase with error: '{e}'")
        return False

    return True

def test_eth_mining(url: str) -> bool:
    """
        Returns whether mining is happening. Returns false.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_mining"})
        res = get_result(response)

        if res is not False:
            raise Exception(f"Bad eth_mining return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_mining with error: '{e}'")
        return False

    return True

def test_eth_accounts(url: str) -> bool:
    """
        Returns a list of addresses owned by client.
        Should be always empty list.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_accounts"})
        res = get_result(response)

        if res != "":
            raise Exception(f"Bad eth_accounts return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_accounts with error: '{e}'")
        return False

    return True

def test_eth_blockNumber(url: str) -> bool:
    """
        Returns the block number. Should be non zero and should increment over time.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_blockNumber"})
        res = get_result(response)

        if "0x" not in res.lower():
            raise Exception(f"Bad eth_blockNumber return value: {res}")

        time.sleep(BLOCK_TIME_S * 2)

        response2 = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_blockNumber"})
        res2 = get_result(response2)

        if not int(res, base=16) < int(res2, base=16):
            raise Exception(f"Bad eth_blockNumber, did not increment: {res} vs {res2}")

    except Exception as e:
        print(f"Failed test test_eth_blockNumber with error: '{e}'")
        return False

    return True

def test_eth_getBlockTransactionCountByHash(url: str) -> bool:
    """
        Returns the information about a transaction requested by transaction hash.
        In the response object, `blockHash`, `blockNumber`, and `transactionIndex` are `null`
        when the transaction is pending.
        We will assume the TX is not pending for this test
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockTransactionCountByHash"})
        res = get_result(response)

        if "0x" not in res.lower():
            raise Exception(f"Bad eth_getBlockTransactionCountByHash return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getBlockTransactionCountByHash with error: '{e}'")
        return False

    return True

def test_eth_getBlockTransactionCountByNumber(url: str) -> bool:
    """
        Returns the information about a transaction requested by transaction hash.
        In the response object, `blockHash`, `blockNumber`, and `transactionIndex` are `null`
        when the transaction is pending.
        We will assume the TX is not pending for this test
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockTransactionCountByNumber"})
        res = get_result(response)

        if "0x" not in res.lower():
            raise Exception(f"Bad eth_getBlockTransactionCountByNumber return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getBlockTransactionCountByNumber with error: '{e}'")
        return False

    return True

def test_eth_getUncleCountByBlockHash(url: str) -> bool:
    """
        Returns the number of uncles in a block matching the given block hash - so 0.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getUncleCountByBlockHash",
                                            "params":["0xb3b20624f8f0f86eb50dd04688409e5cea4bd02d700bf6e79e9384d47d6a5a35"]})
        res = get_result(response)

        if res.lower() != "0x0":
            raise Exception(f"Bad eth_getUncleCountByBlockHash return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getUncleCountByBlockHash with error: '{e}'")
        return False

    return True

def test_eth_getUncleCountByBlockNumber(url: str) -> bool:
    """
        Returns the number of uncles in a block matching the give block number - so 0
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getUncleCountByBlockNumber",
                                            "params":["0xe8"]})
        res = get_result(response)

        if res.lower() != "0x0":
            raise Exception(f"Bad eth_getUncleCountByBlockNumber return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getUncleCountByBlockNumber with error: '{e}'")
        return False

    return True

def test_eth_getBlockByHash(url: str) -> bool:
    """
        Returns information about a block by hash.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getBlockByHash",
                                            "params": [
                                                "0xc0f4906fea23cf6f3cce98cb44e8e1449e455b28d684dfa9ff65426495584de6",
                                                True
                                            ]})
        res = get_result(response)

        if res.lower() != "0x0":
            raise Exception(f"Bad eth_getBlockByHash return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getBlockByHash with error: '{e}'")
        return False

    return True

def test_eth_getBlockByNumber(url: str) -> bool:
    """
        Returns information about a block by number.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_blockNumber"})
        res = get_result(response)

        if "0x" not in res.lower():
            raise Exception(f"Did not get block height for use in block by number test")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getBlockByNumber",
                                            "params": [
                                                res,
                                                True
                                            ]})
        res = get_result(response)

        if res.lower() != "0x0":
            raise Exception(f"Bad eth_getBlockByNumber return value: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getBlockByNumber with error: '{e}'")
        return False

    return True

def test_eth_getUncleByBlockHashAndIndex(url: str) -> bool:
    """
        Returns information about an uncle of a block by hash and uncle index position. Always return null.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getUncleByBlockHashAndIndex",
                                            "params":["0xb3b20624f8f0f86eb50dd04688409e5cea4bd02d700bf6e79e9384d47d6a5a35",
                                                      "0x0"]})
        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get None/null for uncle call")

    except Exception as e:
        print(f"Failed test test_eth_getBlockByNumber with error: '{e}'")
        return False

    return True

def test_eth_getUncleByBlockNumberAndIndex(url: str) -> bool:
    """
        Returns information about an uncle of a block by number and uncle index position. Always return null.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getUncleByBlockNumberAndIndex",
                                            "params":["0x29c",
                                                      "0x0"]})
        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get None/null for uncle call (by index)")

    except Exception as e:
        print(f"Failed test test_eth_getUncleByBlockNumberAndIndex with error: '{e}'")
        return False

    return True

def test_eth_getCompilers(url: str) -> bool:
    """
        Returns information about an uncle of a block by number and uncle index position. Always return null.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getCompilers"})
        res = get_result(response)

        if res is not []:
            raise Exception(f"Did not get empty list for eth get compilers")

    except Exception as e:
        print(f"Failed test test_eth_getCompilers with error: '{e}'")
        return False

    return True

def test_eth_compileSolidity(url: str) -> bool:
    """
        Always return null.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_compileSolidity"})
        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get None/null for solidity compile call (by index)")

    except Exception as e:
        print(f"Failed test test_eth_compileSolidity with error: '{e}'")
        return False

    return True

def test_eth_compile(url: str) -> bool:
    """
        Always return null.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_compile"})
        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get None/null for solidity call")

    except Exception as e:
        print(f"Failed test test_eth_compile with error: '{e}'")
        return False

    return True

def test_eth_compileSerpent(url: str) -> bool:
    """
        Always return empty string.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_compileSerpent", "params": "0x000"})
        res = get_result(response)

        if res != "":
            raise Exception(f"Did not get empty string for compile serpent")

    except Exception as e:
        print(f"Failed test test_eth_compile with error: '{e}'")
        return False

    return True

def test_eth_hashrate(url: str) -> bool:
    """
        Return the number of hashes per second the node is mining with. Always return 0x1.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_hashrate"})
        res = get_result(response)

        if res != "0x1":
            raise Exception(f"Did not get 1 for hashrate. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_hashrate with error: '{e}'")
        return False

    return True

def test_eth_gasPrice(url: str) -> bool:
    """
        Return the gas price in wei.
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_gasPrice", "params": [] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for gasPrice. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_gasPrice with error: '{e}'")
        return False

    return True

def test_eth_newFilter(url: str) -> bool:
    """
        Creates a filter object. Not yet implemented so will return null
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_newFilter", "params": []})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_newFilter with error: '{e}'")
        return False

    return True

def test_eth_newBlockFilter(url: str) -> bool:
    """
        Creates a block filter object. Not yet implemented so will return null
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_newBlockFilter", "params": []})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_newBlockFilter with error: '{e}'")
        return False

    return True

def test_eth_newPendingTransactionFilter(url: str) -> bool:
    """
        Creates a pending transaction filter object. Not yet implemented so will return null
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_newPendingTransactionFilter", "params": []})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_newPendingTransactionFilter with error: '{e}'")
        return False

    return True

def test_eth_uninstallFilter(url: str) -> bool:
    """
        Creates a pending transaction filter object. Not yet implemented so will return null
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_uninstallFilter", "params": []})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_uninstallFilter with error: '{e}'")
        return False

    return True

def test_eth_getFilterChanges(url: str) -> bool:
    """
        Get filter changes. Returns null for now
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getFilterChanges", "params": []})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_getFilterChanges with error: '{e}'")
        return False

    return True

def test_eth_getFilterLogs(url: str) -> bool:
    """
        Get filter logs. Returns null for now
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getFilterLogs", "params": []})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_getFilterLogs with error: '{e}'")
        return False

    return True

def test_eth_getLogs(url: str) -> bool:
    """
        Get logs. Returns null for now
    """
    try:
        # Get the block at the head of the chain
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getLogs"})
        res = get_result(response)
    except Exception as e:
        print(f"Failed test test_eth_getLogs with error: '{e}'")
        return False

    return True

def test_eth_subscribe(url: str) -> bool:
    """
        Subscribe to a websocket. TODO for now.
    """
    return True

def test_eth_unsubscribe(url: str) -> bool:
    """
        Unsubscribe to a websocket. TODO for now.
    """
    return True

def test_eth_call(url: str) -> bool:
    """
        Executes a new message call immediately without creating a transaction on the block chain.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_call", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_call. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_call with error: '{e}'")
        return False

    return True

def test_eth_estimateGas(url: str) -> bool:
    """
        Generates and returns an estimate of how much gas is necessary to allow the transaction to complete.
        The transaction will not be added to the blockchain. Note that the estimate may be significantly
        more than the amount of gas actually used by the transaction,
        for a variety of reasons including EVM mechanics and node performance."
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_estimateGas", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_estimateGas. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_estimateGas with error: '{e}'")
        return False

    return True

def test_eth_getTransactionCount(url: str) -> bool:
    """
        Returns the number of transactions sent from an address.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionCount", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_getTransactionCount. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getTransactionCount with error: '{e}'")
        return False

    return True

def test_eth_getTransactionByHash(url: str) -> bool:
    """
        Returns the information about a transaction requested by transaction hash.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByHash", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_getTransactionByHash. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getTransactionByHash with error: '{e}'")
        return False

    return True

def test_eth_getTransactionByBlockHashAndIndex(url: str) -> bool:
    """
        Returns information about a transaction by block hash and transaction index position.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByBlockHashAndIndex", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_getTransactionByBlockHashAndIndex. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getTransactionByBlockHashAndIndex with error: '{e}'")
        return False

    return True

def test_eth_getTransactionByBlockNumberAndIndex(url: str) -> bool:
    """
        Returns information about a transaction by block number and transaction index position.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByBlockNumberAndIndex", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_getTransactionByBlockNumberAndIndex. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getTransactionByBlockNumberAndIndex with error: '{e}'")
        return False

    return True

def test_eth_getTransactionReceipt(url: str) -> bool:
    """
        Returns the receipt of a transaction by transaction hash.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": ["latest"] })
        res = get_result(response)

        if res != "0x123":
            raise Exception(f"Did not get 1 for eth_getTransactionReceipt. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getTransactionReceipt with error: '{e}'")
        return False

    return True

def test_eth_sign(url: str) -> bool:
    """
        Not implemented since this requires the node to have your private key.

        The sign method calculates an Ethereum specific signature with:
        sign(keccak256(""\x19Ethereum Signed Message:\n"" + len(message) + message))).

        By adding a prefix to the message makes the calculated signature recognisable
        as an Ethereum specific signature. This prevents misuse where a malicious DApp can
         sign arbitrary data (e.g. transaction) and use the signature to impersonate the victim.

        Note the address to sign with must be unlocked."
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sign",
                                            "params": ["0x9b2055d370f73ec7d8a03e965129118dc8f5bf83", "0xdeadbeaf"] })
        res = get_result(response)

        if res != []:
            raise Exception(f"Did not get [] for eth_sign. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_sign with error: '{e}'")
        return False

    return True

def test_eth_signTransaction(url: str) -> bool:
    """
        Not implemented since this requires the node to have your private key.

        Signs a transaction that can be submitted to the network
        at a later time using with eth_sendRawTransaction.
        Is this just a local API, not really an RPC call?"
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_signTransaction",
                                            "params": [{"data":"0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675","from": "0xb60e8dd61c5d32be8058bb8eb970870f07233155","gas": "0x76c0","gasPrice": "0x9184e72a000","to": "0xd46e8dd67c5d32be8058bb8eb970870f07244567","value": "0x9184e72a"}] })
        res = get_result(response)

        if res != "":
            raise Exception(f"Did not get '' for eth_signTransaction. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_signTransaction with error: '{e}'")
        return False

    return True

def test_eth_sendTransaction(url: str) -> bool:
    """
        Not implemented since this requires the node to have your private key.

        Creates new message call transaction or a contract creation, if the data field contains code.
    """
    try:


        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sendTransaction",
                                            "params": [{ "from": "0xb60e8dd61c5d32be8058bb8eb970870f07233155", "to": "0xd46e8dd67c5d32be8058bb8eb970870f07244567", "gas": "0x76c0", "gasPrice": "0x9184e72a000", "value": "0x9184e72a", "data": "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675" }] })
        res = get_result(response)

        if res != "":
            raise Exception(f"Did not get '' for eth_sendTransaction. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_sendTransaction with error: '{e}'")
        return False

    return True

def test_eth_sendRawTransaction(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Creates new message call transaction or a contract creation, if the data field contains code. Raw transactions
        are in the RLP data format.
    """
    try:
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': "0xaaB168343e743141A10CE6C3D454454A7D522506",
            'from':account.address,
            'value':int(0),
            'data':"",
            'gas':GAS_LIMIT,
            'gasPrice':int(GAS_PRICE*(10**9)),
            'chainId':CHAIN_ID,
            'nonce':int(nonce)
        }

        signed_transaction = account.signTransaction(transaction)

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sendRawTransaction",
                                            "params": [signed_transaction.rawTransaction.hex()]})

        res = get_result(response)

        if res == "" or res is None:
            raise Exception(f"Did not get TX receipt for eth_sendRawTransaction. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_sendRawTransaction with error: '{e}'")
        return False

    return True

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

def test_eth_chainId(url: str) -> bool:
    """
        Test the chain ID can be retrieved, and is correct
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_chainId"})

        res = get_result(response)

        if "0x814d" not in res.lower():
            raise Exception(f"Bad chain ID: {res}")

    except Exception as e:
        print(f"Failed test test_eth_chainId with error: '{e}'")
        return False

    return True

def parse_commandline():
    parser = argparse.ArgumentParser()
    parser.add_argument('--api', type=str, required=True, help='API to test against')
    parser.add_argument('--private-key-genesis', type=str, help='Private key found in genesis',
                        default="db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3")
    parser.add_argument('--private-key-test', type=str, help='Private key to move genesis funds to, for test usage',
                        default="a8b68f4800bc7513fca14a752324e41b2fa0a7c06e80603aac9e5961e757d906")

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

    ret = True

    ret &= test_move_funds(args.api, genesis_privkey, account, api)
    #ret &= test_eth_chainId(args.api)
    #ret &= test_eth_blockNumber(args.api)
    #ret &= test_eth_feeHistory(args.api) # todo: implement fully or decide it is a no-op
    #ret &= test_eth_getCode(args.api, account, w3)
    ret &= test_eth_getStorageAt(args.api, account, w3)
    #ret &= test_eth_getProof(args.api)
    #ret &= test_eth_getBalance(args.api)
    #ret &= test_web3_clientVersion(args.api)
    #ret &= test_web3_sha3(args.api)
    #ret &= test_net_version(args.api)
    #ret &= test_net_listening(args.api)
    #ret &= test_net_peerCount(args.api)
    #ret &= test_eth_protocolVersion(args.api)
    #ret &= test_eth_syncing(args.api)
    #ret &= test_eth_coinbase(args.api)
    #ret &= test_eth_mining(args.api)
    #ret &= test_eth_accounts(args.api)
    #ret &= test_eth_getBlockTransactionCountByNumber(args.api)
    #ret &= test_eth_getUncleCountByBlockHash(args.api)
    #ret &= test_eth_getUncleCountByBlockNumber(args.api)
    ##ret &= test_eth_getBlockByHash(args.api)
    ##ret &= test_eth_getBlockByNumber(args.api)
    #ret &= test_eth_getUncleByBlockHashAndIndex(args.api)
    #ret &= test_eth_getUncleByBlockNumberAndIndex(args.api)
    #ret &= test_eth_getCompilers(args.api)
    #ret &= test_eth_compileSolidity(args.api)
    #ret &= test_eth_compile(args.api)
    #ret &= test_eth_compileSerpent(args.api)
    #ret &= test_eth_hashrate(args.api)
    #ret &= test_eth_gasPrice(args.api)
    #ret &= test_eth_newFilter(args.api)
    #ret &= test_eth_newBlockFilter(args.api)
    #ret &= test_eth_newPendingTransactionFilter(args.api)
    #ret &= test_eth_uninstallFilter(args.api)
    #ret &= test_eth_getFilterChanges(args.api)
    #ret &= test_eth_getFilterLogs(args.api)
    #ret &= test_eth_getLogs(args.api)
    #ret &= test_eth_subscribe(args.api)
    #ret &= test_eth_unsubscribe(args.api)
    #ret &= test_eth_call(args.api)
    #ret &= test_eth_estimateGas(args.api)
    #ret &= test_eth_getTransactionCount(args.api)
    #ret &= test_eth_getTransactionByHash(args.api)
    #ret &= test_eth_getTransactionByBlockHashAndIndex(args.api)
    #ret &= test_eth_getTransactionByBlockNumberAndIndex(args.api)
    #ret &= test_eth_getTransactionReceipt(args.api)
    ret &= test_eth_sign(args.api)
    #ret &= test_eth_signTransaction(args.api)
    #ret &= test_eth_sendTransaction(args.api)
    ret &= test_eth_sendRawTransaction(args.api, account, w3)

    #ret &= test_eth_getBlockTransactionCountByHash(args.api)

    if not ret:
        print(f"Test failed")
        sys.exit(1)
    else:
        print(f"Test passed!")


if __name__ == '__main__':
    main()
