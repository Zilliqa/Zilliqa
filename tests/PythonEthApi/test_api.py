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

def checkHasField(obj, field, is_hex = False):
    if not field in obj:
        raise Exception(f"{obj} is missing field {field}")

    if is_hex and not (obj[field] is None or "0x" in obj[field] or obj[field] == ""):
        raise Exception(f"{obj} has field {field} but it is not a hex value")

def checkIsTransaction(obj):
    checkHasField(obj, "hash", True)
    checkHasField(obj, "nonce", True)
    checkHasField(obj, "blockHash", True)
    checkHasField(obj, "blockNumber", True)
    checkHasField(obj, "transactionIndex", True)
    checkHasField(obj, "from", True)
    checkHasField(obj, "to", True)
    checkHasField(obj, "value", True)
    checkHasField(obj, "gasPrice", True)
    checkHasField(obj, "gas", True)
    checkHasField(obj, "input", True)
    checkHasField(obj, "v", True)
    #checkHasField(obj, "standardV", True) # TODO(HUT): missing
    #checkHasField(obj, "raw", True) # TODO(HUT): missing
    #checkHasField(obj, "publicKey", True) # TODO(HUT): missing
    checkHasField(obj, "chainId", True)

def checkIsTransactionReceipt(obj):
    checkHasField(obj, "transactionHash", True)
    checkHasField(obj, "transactionIndex", True)
    checkHasField(obj, "from", True)
    checkHasField(obj, "to", True)
    checkHasField(obj, "blockHash", True)
    checkHasField(obj, "blockNumber", True)
    checkHasField(obj, "cumulativeGasUsed", True)
    checkHasField(obj, "gasUsed", True)
    checkHasField(obj, "contractAddress", True)
    checkHasField(obj, "logs")
    checkHasField(obj, "logsBloom", True)
    #checkHasField(obj, "value", True) # TODO(HUT): missing
    checkHasField(obj, "v", True)
    checkHasField(obj, "r", True)
    checkHasField(obj, "s", True)

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


def test_eth_feeHistory(url: str) -> bool:
    """
      Returns a collection of historical gas information from which you can decide what
       to submit as your maxFeePerGas and/or maxPriorityFeePerGas. This method
        was introduced with EIP-1559. https://docs.alchemy.com/alchemy/apis/ethereum/eth-feehistory
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_feeHistory", "params": ["0x4", "latest"] })
        get_result(response)
    except Exception as e:
        print(f"********* Failed test test_eth_feeHistory with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
            'to': "",
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

        if "0x" not in res:
            res = "0x" + res

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getStorageAt",
                                            "params": [res, "0x1", "latest"]})

        res = get_result(response)

        if "0x0000000000000000000000000000000000000000000000000000000000000401" != res.lower():
            raise Exception(f"Failed to retrieve correct contract amount: {res} when expected 0x0000000000000000000000000000000000000000000000000000000000000401")

    except Exception as e:
        print(f"********* Failed test test_eth_getStorageAt with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
            'to': "",
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

        if "0x" not in res:
            res = "0x" + res

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getCode",
                                            "params": [res, "latest"]})

        res = get_result(response)

        if code[0:4] != res[0:4].lower():
            raise Exception(f"Failed to retrieve correct contract amount: {res} when expected {code}")

    except Exception as e:
        print(f"********* Failed test test_eth_getCode with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_getProof with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getBalance(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the balance of the account of a given address.
    """
    try:
        # Just use the base w3 api
        amount = w3.eth.getBalance(account.address)

        if not amount > 0:
            raise Exception(f"Expected account to have some funds but instead had {amount}")

    except Exception as e:
        print(f"********* Failed test test_eth_getBalance with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_web3_clientVersion(url: str) -> bool:
    """
        Returns the current client version.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "web3_clientVersion"})
        res = get_result(response)

        if "Zilliqa/v8.2" not in res:
            raise Exception(f"Bad client version: {res}")

    except Exception as e:
        print(f"********* Failed test eth_web3_clientVersion with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_web3_sha3(url: str) -> bool:
    """
        Returns the string provided as a sha3.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "web3_sha3", "params":["0x68656c6c6f20776f726c64"]})
        res = get_result(response)

        if "0x47173285a8d7341e5e972fc677286384f802f8ef42a5ec5f03bbfa254cb01fad" not in res.lower():
            raise Exception(f"Bad sha3 return value: {res}")

    except Exception as e:
        print(f"********* Failed test test_web3_sha3 with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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

        # Just test the response is a hex value
        if "0x" not in res.lower():
            raise Exception(f"Bad net_version return value: {res}")

    except Exception as e:
        print(f"********* Failed test test_net_version with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_net_listening(url: str) -> bool:
    """
        Returns true if client is actively listening for network connections. Always false
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "net_listening"})
        res = get_result(response)

        if res is not True:
            raise Exception(f"Bad net_listening return value: {res}")

    except Exception as e:
        print(f"********* Failed test test_net_listening with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_net_peerCount with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_protocolVersion(url: str) -> bool:
    """
        will always return ''
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_protocolVersion"})
        res = get_result(response)

        if res != "0x41":
            raise Exception(f"Bad eth_protocolVersion return value: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_protocolVersion with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_syncing with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_mining with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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

        if res != []:
            raise Exception(f"Bad eth_accounts return value: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_accounts with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
            raise Exception(f"Bad eth_blockNumber, did not increment: {res} vs {res2} - this is not an error on the isolated server.")

    except Exception as e:
        print(f"********* Failed test test_eth_blockNumber with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getBlockTransactionCountByHash(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the number of transactions in a block from a block matching the given block hash.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [res] })

        res = get_result(response)

        if "blockHash" not in res:
            raise Exception(f"Did not find block hash from TX receipt {res}")

        if res == "" or res is None:
            raise Exception(f"Did not get TX receipt for eth_sendRawTransaction. Got: {res}")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockTransactionCountByHash", "params": [res["blockHash"]] })

        res = get_result(response)

        if not int(res, 16) > 0:
            raise Exception(f"Did not get enough TXs in block: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_getBlockTransactionCountByHash with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getBlockTransactionCountByNumber(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the number of TXs in a block by the block number
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [res] })

        res = get_result(response)

        if "blockNumber" not in res:
            raise Exception(f"Did not find block number from TX receipt")

        if res == "" or res is None:
            raise Exception(f"Did not get TX receipt for eth_sendRawTransaction. Got: {res}")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockTransactionCountByNumber", "params": [res["blockNumber"]] })

        res = get_result(response)

        if not int(res, 16) > 0:
            raise Exception(f"Did not get enough TXs in block: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_getBlockTransactionCountByNumber with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_getUncleCountByBlockHash with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_getUncleCountByBlockNumber with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getBlockByHash(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns information about a block by hash.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [res] })

        res = get_result(response)

        if "blockHash" not in res:
            raise Exception(f"Did not find block hash from TX receipt")

        if res == "" or res is None:
            raise Exception(f"Did not get TX receipt for eth_sendRawTransaction. Got: {res}")

        # First path: Full TX formats
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockByHash", "params": [res["blockHash"], True] })

        res = get_result(response)
        checkHasField(res, "number", True)
        checkHasField(res, "hash", True)
        checkHasField(res, "parentHash", True)
        checkHasField(res, "nonce", True)
        checkHasField(res, "sha3Uncles", True)
        checkHasField(res, "logsBloom", True)
        checkHasField(res, "transactionsRoot", True)
        checkHasField(res, "stateRoot", True)
        checkHasField(res, "receiptsRoot", True)
        checkHasField(res, "miner", True)
        checkHasField(res, "difficulty", True)
        checkHasField(res, "totalDifficulty", True)
        checkHasField(res, "extraData", True)
        checkHasField(res, "size", True)
        checkHasField(res, "gasLimit", True)
        checkHasField(res, "gasUsed", True)
        checkHasField(res, "timestamp", True)
        checkHasField(res, "transactions")
        checkHasField(res, "uncles")

        for transaction in res["transactions"]:
            checkIsTransaction(transaction)

    except Exception as e:
        print(f"********* Failed test test_eth_getBlockByHash with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getBlockByNumber(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns information about a block by number.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [res] })

        res = get_result(response)

        if "blockNumber" not in res:
            raise Exception(f"Did not find block number from TX receipt")

        if res == "" or res is None:
            raise Exception(f"Did not get TX receipt for eth_sendRawTransaction. Got: {res}")

        # First path: Full TX formats
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockByNumber", "params": [res["blockNumber"], True] })

        res = get_result(response)
        checkHasField(res, "number", True)
        checkHasField(res, "hash", True)
        checkHasField(res, "parentHash", True)
        #checkHasField(res, "nonce", True) # TODO(HUT): missing
        checkHasField(res, "sha3Uncles", True)
        checkHasField(res, "logsBloom", True)
        #checkHasField(res, "transactionsRoot", True) # TODO(HUT): missing
        checkHasField(res, "stateRoot", True)
        #checkHasField(res, "receiptsRoot", True) # TODO(HUT): missing
        checkHasField(res, "miner", True)
        checkHasField(res, "difficulty", True)
        #checkHasField(res, "totalDifficulty", True) # TODO(HUT): missing
        checkHasField(res, "extraData", True)
        checkHasField(res, "size", True)
        checkHasField(res, "gasLimit", True)
        checkHasField(res, "gasUsed", True)
        checkHasField(res, "timestamp", True)
        checkHasField(res, "transactions")
        checkHasField(res, "uncles")

        for transaction in res["transactions"]:
            checkIsTransaction(transaction)

    except Exception as e:
        print(f"********* Failed test test_eth_getBlockByNumber with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getUncleByBlockHashAndIndex(url: str) -> bool:
    """
        Returns information about an uncle of a block by hash and uncle index position. Always return null.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getUncleByBlockHashAndIndex",
                                            "params":["0x0", "0x0"]})
        res = get_result(response)

        if res is not None:
            raise Exception(f"Bad eth_getUncleByBlockHashAndIndex return value: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_getBlockByNumber with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getUncleByBlockNumberAndIndex(url: str) -> bool:
    """
        Returns information about an uncle of a block by number and uncle index position. Always return null.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0",
                                            "method": "eth_getUncleByBlockNumberAndIndex",
                                            "params":["0x0", "0x0"]})
        res = get_result(response)

        if res is not None:
            raise Exception(f"Bad eth_getUncleByBlockNumberAndIndex return value: {res}")
    except Exception as e:
        print(f"********* Failed test test_eth_getUncleByBlockNumberAndIndex with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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

        if "0x" not in res:
            raise Exception(f"Did not get hex value for gasPrice. Got: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_gasPrice with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_newFilter with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_newBlockFilter with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_newPendingTransactionFilter with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_uninstallFilter with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_getFilterChanges with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_getFilterLogs with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_getLogs with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_call with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_estimateGas with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getTransactionCount(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the number of transactions sent from an address.
    """
    try:
        nonce = w3.eth.getTransactionCount(account.address)
        unique_address = "0xaaB168343e743141A10CE6C3D454454A7D522506"

        transaction = {
            'to': account.address,
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

        nonce_after = w3.eth.getTransactionCount(account.address)

        if nonce_after != nonce + 1:
            raise Exception(f"Did not get nonce increment for sent TX. Got: {nonce_after} and {nonce}")

        nonce = w3.eth.getTransactionCount(unique_address)

        if nonce != 0:
            raise Exception(f"Did not get nonce of 0 for unique address. Got: {nonce}")

    except Exception as e:
        print(f"********* Failed test test_eth_getTransactionCount with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getTransactionByHash(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the information about a transaction requested by transaction hash.
    """
    try:
        # Submit a normal transaction to self, then use the response to get the TX by hash
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByHash",
                                            "params": [res]})

        res = get_result(response)
        checkIsTransaction(res)

        # Now test failure case
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByHash",
                                            "params": ["0x1"]})

        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get null response for missing TX eth_getTransactionByHash. Got: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_getTransactionByHash with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getTransactionByBlockHashAndIndex(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns information about a transaction by block hash and transaction index position.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        tx_hash = get_result(response)

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [tx_hash] })

        res = get_result(response)

        if "blockHash" not in res:
            raise Exception(f"Did not find block hash from TX receipt")

        if "transactionIndex" not in res:
            raise Exception(f"Did not find index from TX receipt {res}")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByBlockHashAndIndex", "params": [res["blockHash"], res["transactionIndex"]] })

        res = get_result(response)

        checkIsTransaction(res)

        if tx_hash != res["hash"]:
            raise Exception(f"Did not find matching TX hash when retrieving TX! {tx_hash} vs {res['hash']}")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByBlockHashAndIndex", "params": ["0x0", "0x0"] })

        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get null when searching for non existent TX: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_getTransactionByBlockHashAndIndex with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getTransactionByBlockNumberAndIndex(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns information about a transaction by block number and transaction index position.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        tx_hash = get_result(response)

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [tx_hash] })

        res = get_result(response)

        if "blockNumber" not in res:
            raise Exception(f"Did not find block hash from TX receipt")

        if "transactionIndex" not in res:
            raise Exception(f"Did not find index from TX receipt {res}")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByBlockNumberAndIndex", "params": [res["blockNumber"], res["transactionIndex"]] })

        res = get_result(response)

        checkIsTransaction(res)

        if tx_hash != res["hash"]:
            raise Exception(f"Did not find matching TX hash when retrieving TX! {tx_hash} vs {res['hash']}")

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionByBlockNumberAndIndex", "params": ["0x0", "0x0"] })

        res = get_result(response)

        if res is not None:
            raise Exception(f"Did not get null when searching for non existent TX: {res}")


    except Exception as e:
        print(f"********* Failed test test_eth_getTransactionByBlockNumberAndIndex with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_getTransactionReceipt(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the receipt of a transaction by transaction hash.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        tx_hash = get_result(response)

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [tx_hash] })

        res = get_result(response)

        checkIsTransactionReceipt(res)

    except Exception as e:
        print(f"********* Failed test test_eth_getTransactionReceipt with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_sign with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_signTransaction with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_sendTransaction with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_eth_sendRawTransaction with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
        print(f"********* Failed test test_move_funds with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_chainId(url: str) -> bool:
    """
        Test the chain ID can be retrieved, and is correct
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_chainId"})

        res = get_result(response)

        if hex(CHAIN_ID) not in res.lower():
            raise Exception(f"Bad chain ID: {res}")

    except Exception as e:
        print(f"********* Failed test test_eth_chainId with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def test_eth_recoverTransaction(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the receipt of a transaction by transaction hash.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
            'from':account.address,
            'value':int(0),
            'data':"",
            'gas':GAS_LIMIT,
            'gasPrice':int(GAS_PRICE*(10**9)),
            'chainId':CHAIN_ID,
            'nonce':int(nonce)
        }

        signed_transaction = account.signTransaction(transaction)
        rawTx = signed_transaction.rawTransaction.hex()

        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_recoverTransaction",
                                            "params": [rawTx]})

        originalSender = get_result(response)

        ## Here rely on another api call to find the block the TX was in.
        #response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [tx_hash] })
        #res = get_result(response)
        #checkIsTransactionReceipt(res)

        if originalSender != account.address:
            raise Exception(f"Did not get back the original sender. Received: {originalSender} Expected: {account.address}")

    except Exception as e:
        print(f"********* Failed test test_eth_recoverTransaction with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
        return False

    return True

def eth_getBlockReceipts(url: str, account: eth_account.signers.local.LocalAccount, w3: Web3) -> bool:
    """
        Returns the receipt of a transaction by transaction hash.
    """
    try:
        # Submit a normal transaction to self
        nonce = w3.eth.getTransactionCount(account.address)

        transaction = {
            'to': account.address,
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

        # Here rely on another api call to find the block the TX was in.
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getTransactionReceipt", "params": [res] })

        res = get_result(response)

        if "blockHash" not in res:
            raise Exception(f"Did not find block hash from TX receipt")

        if res == "" or res is None:
            raise Exception(f"Did not get TX receipt for eth_sendRawTransaction. Got: {res}")

        # First path: Full TX formats
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getBlockReceipts", "params": [res["blockHash"]] })

        res = get_result(response)

        if not isinstance(res, list):
            raise Exception(f"Did not get a list response: {res}")

        if len(res) <= 0:
            raise Exception(f"Did not get anything in the list response: {res}")

        checkIsTransactionReceipt(res[0])

    except Exception as e:
        print(f"********* Failed test eth_getBlockReceipts with error: '{e}'")
        print(f"\n\nTraceback: {traceback.format_exc()}")
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
    ret &= test_eth_chainId(args.api)
    ret &= test_eth_feeHistory(args.api) # todo: implement fully or decide it is a no-op
    ret &= test_eth_getStorageAt(args.api, account, w3)
    ret &= test_eth_getCode(args.api, account, w3)
    ret &= test_eth_getBalance(args.api, account, w3) # Not properly tested
    #ret &= test_eth_getProof(args.api) # TODO(HUT): implement
    ret &= test_web3_clientVersion(args.api)
    ret &= test_web3_sha3(args.api)
    ret &= test_net_version(args.api)
    ret &= test_net_listening(args.api)
    ret &= test_net_peerCount(args.api)
    ret &= test_eth_protocolVersion(args.api)
    ret &= test_eth_syncing(args.api)
    ret &= test_eth_mining(args.api)
    ret &= test_eth_accounts(args.api)
    #ret &= test_eth_blockNumber(args.api)
    ret &= test_eth_getBlockTransactionCountByHash(args.api, account, w3)
    ret &= test_eth_getBlockTransactionCountByNumber(args.api, account, w3)
    ret &= test_eth_getUncleCountByBlockNumber(args.api)
    ret &= test_eth_getUncleCountByBlockHash(args.api)
    ret &= test_eth_getBlockByHash(args.api, account, w3)
    ret &= test_eth_getBlockByNumber(args.api, account, w3)
    ret &= test_eth_getUncleByBlockHashAndIndex(args.api)
    ret &= test_eth_getUncleByBlockNumberAndIndex(args.api)
    ret &= test_eth_gasPrice(args.api)
    #ret &= test_eth_newFilter(args.api) # Only on isolated server?
    #ret &= test_eth_newBlockFilter(args.api)
    #ret &= test_eth_newPendingTransactionFilter(args.api)
    #ret &= test_eth_uninstallFilter(args.api)
    #ret &= test_eth_getFilterChanges(args.api)
    #ret &= test_eth_getFilterLogs(args.api)
    #ret &= test_eth_getLogs(args.api)
    #ret &= test_eth_subscribe(args.api)
    ret &= test_eth_unsubscribe(args.api)
    #ret &= test_eth_call(args.api)
    #ret &= test_eth_estimateGas(args.api)
    ret &= test_eth_getTransactionCount(args.api, account, w3)
    ret &= test_eth_getTransactionByHash(args.api, account, w3)
    ret &= test_eth_getTransactionByBlockHashAndIndex(args.api, account, w3)
    ret &= test_eth_getTransactionByBlockNumberAndIndex(args.api, account, w3)
    ret &= test_eth_getTransactionReceipt(args.api, account, w3)
    ret &= test_eth_sendRawTransaction(args.api, account, w3)
    #ret &= test_eth_sign(args.api)
    #ret &= test_eth_signTransaction(args.api)
    #ret &= test_eth_sendTransaction(args.api)

    # Non-standard (for fireblocks)
    ret &= test_eth_recoverTransaction(args.api, account, w3)
    ret &= eth_getBlockReceipts(args.api, account, w3)

    if not ret:
        print(f"Test failed")
        # sys.exit(1)
    else:
        print(f"Test passed!")


if __name__ == '__main__':
    main()
