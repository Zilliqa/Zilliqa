#  Copyright (C) 2019 Zilliqa
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

import sys
import argparse
import requests
import os
import time

FILE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BLOCK_TIME_S = 2

def get_result(response: requests.models.Response) -> any:
    if response.status_code != 200:
        raise Exception(f"Bad status code {response.status_code} - {response.text}")

    res = response.json()

    if "result" not in res.keys():
        raise Exception(f"Bad JSON, no result found: {response.text}")

    return res["result"]

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

def test_eth_getStorageAt(url: str) -> bool:
    """
        Returns the value from a storage position at a given address, or in other words,
        returns the state of the contract's storage, which may not be exposed
        via the contract's methods
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getStorageAt", "params": ["0x295a70b2de5e3953354a6a8344e616ed314d7251", "0x0", "latest"]})
        get_result(response)

    except Exception as e:
        print(f"Failed test test_eth_getStorageAt with error: '{e}'")
        return False

    return True

def test_eth_getCode(url: str)    -> bool:
    """
        Get a code at the specified address. Which is a smart contract address.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getCode", "params":["0xb59f67a8bff5d8cd03f6ac17265c550ed8f33907", "latest"]})
        get_result(response)

    except Exception as e:
        print(f"Failed test test_eth_feeHistory with error: '{e}'")
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

        if res is not "":
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

        if res is not "0x1":
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

        if res is not "0x123":
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

        if res is not "0x123":
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

        if res is not "0x123":
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

        if res is not "0x123":
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

        if res is not "0x123":
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

        if res is not "0x123":
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

        if res is not "0x123":
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

        if res is not "0x123":
            raise Exception(f"Did not get 1 for eth_getTransactionReceipt. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_getTransactionReceipt with error: '{e}'")
        return False

    return True

def test_eth_sign(url: str) -> bool:
    """
        The sign method calculates an Ethereum specific signature with:
        sign(keccak256(""\x19Ethereum Signed Message:\n"" + len(message) + message))).

        By adding a prefix to the message makes the calculated signature recognisable
        as an Ethereum specific signature. This prevents misuse where a malicious DApp can
         sign arbitrary data (e.g. transaction) and use the signature to impersonate the victim.

        Note the address to sign with must be unlocked."
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sign",
                                            "params": ["latest"] })
        res = get_result(response)

        if res is not "":
            raise Exception(f"Did not get 1 for eth_sign. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_sign with error: '{e}'")
        return False

    return True

def test_eth_signTransaction(url: str) -> bool:
    """
        Signs a transaction that can be submitted to the network
        at a later time using with eth_sendRawTransaction.
        Is this just a local API, not really an RPC call?"
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_signTransaction",
                                            "params": ["latest"] })
        res = get_result(response)

        if res is not "":
            raise Exception(f"Did not get 1 for eth_signTransaction. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_signTransaction with error: '{e}'")
        return False

    return True

def test_eth_sendTransaction(url: str) -> bool:
    """
        Creates new message call transaction or a contract creation, if the data field contains code.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sendTransaction",
                                            "params": ["latest"] })
        res = get_result(response)

        if res is not "":
            raise Exception(f"Did not get 1 for eth_sendTransaction. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_sendTransaction with error: '{e}'")
        return False

    return True

def test_eth_sendRawTransaction(url: str) -> bool:
    """
        Creates new message call transaction or a contract creation, if the data field contains code. Raw transactions
        are in the RLP data format.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_sendRawTransaction",
                                            "params": ["latest"] })
        res = get_result(response)

        if res is not "":
            raise Exception(f"Did not get 1 for eth_sendRawTransaction. Got: {res}")

    except Exception as e:
        print(f"Failed test test_eth_sendRawTransaction with error: '{e}'")
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
    return parser.parse_args()

def main():
    args = parse_commandline()

    print(f"args are {args.api}")

    if args.api[-1] != '/':
        args.api[-1].append('/')

    ret = test_eth_chainId(args.api)
    ret &= test_eth_blockNumber(args.api)
    ret &= test_eth_feeHistory(args.api)
    ret &= test_eth_getStorageAt(args.api)
    ret &= test_eth_getCode(args.api)
    ret &= test_eth_getProof(args.api)
    ret &= test_eth_getBalance(args.api)
    ret &= test_web3_clientVersion(args.api)
    ret &= test_web3_sha3(args.api)
    ret &= test_net_version(args.api)
    ret &= test_net_listening(args.api)
    ret &= test_net_peerCount(args.api)
    ret &= test_eth_protocolVersion(args.api)
    ret &= test_eth_syncing(args.api)
    ret &= test_eth_coinbase(args.api)
    ret &= test_eth_mining(args.api)
    ret &= test_eth_accounts(args.api)
    ret &= test_eth_getBlockTransactionCountByNumber(args.api)
    ret &= test_eth_getUncleCountByBlockHash(args.api)
    ret &= test_eth_getUncleCountByBlockNumber(args.api)
    #ret &= test_eth_getBlockByHash(args.api)
    #ret &= test_eth_getBlockByNumber(args.api)
    ret &= test_eth_getUncleByBlockHashAndIndex(args.api)
    ret &= test_eth_getUncleByBlockNumberAndIndex(args.api)
    ret &= test_eth_getCompilers(args.api)
    ret &= test_eth_compileSolidity(args.api)
    ret &= test_eth_compile(args.api)
    ret &= test_eth_compileSerpent(args.api)
    ret &= test_eth_hashrate(args.api)
    ret &= test_eth_gasPrice(args.api)
    ret &= test_eth_newFilter(args.api)
    ret &= test_eth_newBlockFilter(args.api)
    ret &= test_eth_newPendingTransactionFilter(args.api)
    ret &= test_eth_uninstallFilter(args.api)
    ret &= test_eth_getFilterChanges(args.api)
    ret &= test_eth_getFilterLogs(args.api)
    ret &= test_eth_getLogs(args.api)
    ret &= test_eth_subscribe(args.api)
    ret &= test_eth_unsubscribe(args.api)
    ret &= test_eth_call(args.api)
    ret &= test_eth_estimateGas(args.api)
    ret &= test_eth_getTransactionCount(args.api)
    ret &= test_eth_getTransactionByHash(args.api)
    ret &= test_eth_getTransactionByBlockHashAndIndex(args.api)
    ret &= test_eth_getTransactionByBlockNumberAndIndex(args.api)
    ret &= test_eth_getTransactionReceipt(args.api)
    ret &= test_eth_sign(args.api)
    ret &= test_eth_signTransaction(args.api)
    ret &= test_eth_sendTransaction(args.api)
    ret &= test_eth_sendRawTransaction(args.api)
    ret &= test_eth_getBlockTransactionCountByHash(args.api)

    if not ret:
        print(f"Test failed")
        sys.exit(1)
    else:
        print(f"Test passed!")


if __name__ == '__main__':
    main()
