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

FILE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def test_eth_feeHistory(url: str) -> bool:
    """
      Returns a collection of historical gas information from which you can decide what
       to submit as your maxFeePerGas and/or maxPriorityFeePerGas. This method
        was introduced with EIP-1559. https://docs.alchemy.com/alchemy/apis/ethereum/eth-feehistory
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_feeHistory", "params": [4, "latest", [25, 75]] })

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

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

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

    except Exception as e:
        print(f"Failed test test_eth_feeHistory with error: '{e}'")
        return False

    return True

def test_eth_getCode(url: str)    -> bool:
    """
        Get a code at the specified address. Which is a smart contract address.
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_getCode", "params":["0xb59f67a8bff5d8cd03f6ac17265c550ed8f33907", "latest"]})

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

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

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

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

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

    except Exception as e:
        print(f"Failed test test_eth_getBalance with error: '{e}'")
        return False

    return True

def test_web3_clientVersion(url: str) -> bool:
    return True

def test_web3_sha3(url: str) -> bool:
    return True

def test_net_version(url: str) -> bool:
    return True

def test_net_listening(url: str) -> bool:
    return True

def test_net_peerCount(url: str) -> bool:
    return True

def test_eth_protocolVersion(url: str) -> bool:
    return True

def test_eth_syncing(url: str) -> bool:
    return True

def test_eth_coinbase(url: str) -> bool:
    return True

def test_eth_mining(url: str) -> bool:
    return True

def test_eth_accounts(url: str) -> bool:
    return True

def test_eth_blockNumber(url: str) -> bool:
    return True

def test_eth_getBlockTransactionCountByHash(url: str) -> bool:
    return True

def test_eth_getBlockTransactionCountByNumber(url: str) -> bool:
    return True

def test_eth_getUncleCountByBlockHash(url: str) -> bool:
    return True

def test_eth_getUncleCountByBlockNumber(url: str) -> bool:
    return True

def test_eth_getBlockByHash(url: str) -> bool:
    return True

def test_eth_getBlockByNumber(url: str) -> bool:
    return True

def test_eth_getUncleByBlockHashAndIndex(url: str) -> bool:
    return True

def test_eth_getUncleByBlockNumberAndIndex(url: str) -> bool:
    return True

def test_eth_getCompilers(url: str) -> bool:
    return True

def test_eth_compileSolidity(url: str) -> bool:
    return True

def test_eth_compile(url: str) -> bool:
    return True

def test_eth_compileSerpent(url: str) -> bool:
    return True

def test_eth_hashrate(url: str) -> bool:
    return True

def test_eth_gasPrice(url: str) -> bool:
    return True

def test_eth_newFilter(url: str) -> bool:
    return True

def test_eth_newBlockFilter(url: str) -> bool:
    return True

def test_eth_newPendingTransactionFilter(url: str) -> bool:
    return True

def test_eth_uninstallFilter(url: str) -> bool:
    return True

def test_eth_getFilterChanges(url: str) -> bool:
    return True

def test_eth_getFilterLogs(url: str) -> bool:
    return True

def test_eth_getLogs(url: str) -> bool:
    return True

def test_eth_subscribe(url: str) -> bool:
    return True

def test_eth_unsubscribe(url: str) -> bool:
    return True

def test_eth_call(url: str) -> bool:
    return True

def test_eth_estimateGas(url: str) -> bool:
    return True

def test_eth_getTransactionCount(url: str) -> bool:
    return True

def test_eth_getTransactionByHash(url: str) -> bool:
    return True

def test_eth_getTransactionByBlockHashAndIndex(url: str) -> bool:
    return True

def test_eth_getTransactionByBlockNumberAndIndex(url: str) -> bool:
    return True

def test_eth_getTransactionReceipt(url: str) -> bool:
    return True

def test_eth_sign(url: str) -> bool:
    return True

def test_eth_signTransaction(url: str) -> bool:
    return True

def test_eth_sendTransaction(url: str) -> bool:
    return True

def test_eth_sendRawTransaction(url: str) -> bool:
    return True

def test_eth_chainId(url: str) -> bool:
    """
        Test the chain ID can be retrieved, and is correct
    """
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_chainId"})

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

        res = response.json()["result"]
        if "0x" not in res:
            raise Exception(f"Bad json or response {response.status_code} - {response.text}")

        if "0x66" not in res:
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
    ret |= test_eth_feeHistory(args.api)
    ret |= test_eth_getStorageAt(args.api)
    ret |= test_eth_getCode(args.api)
    ret |= test_eth_getProof(args.api)
    ret |= test_eth_getBalance(args.api)
    ret |= test_web3_clientVersion(args.api)
    ret |= test_web3_sha3(args.api)
    ret |= test_net_version(args.api)
    ret |= test_net_listening(args.api)
    ret |= test_net_peerCount(args.api)
    ret |= test_eth_protocolVersion(args.api)
    ret |= test_eth_syncing(args.api)
    ret |= test_eth_coinbase(args.api)
    ret |= test_eth_mining(args.api)
    ret |= test_eth_accounts(args.api)
    ret |= test_eth_blockNumber(args.api)
    ret |= test_eth_getBlockTransactionCountByHash(args.api)
    ret |= test_eth_getBlockTransactionCountByNumber(args.api)
    ret |= test_eth_getUncleCountByBlockHash(args.api)
    ret |= test_eth_getUncleCountByBlockNumber(args.api)
    ret |= test_eth_getBlockByHash(args.api)
    ret |= test_eth_getBlockByNumber(args.api)
    ret |= test_eth_getUncleByBlockHashAndIndex(args.api)
    ret |= test_eth_getUncleByBlockNumberAndIndex(args.api)
    ret |= test_eth_getCompilers(args.api)
    ret |= test_eth_compileSolidity(args.api)
    ret |= test_eth_compileLLL(args.api)
    ret |= test_eth_compileSerpent(args.api)
    ret |= test_eth_hashrate(args.api)
    ret |= test_eth_gasPrice(args.api)
    ret |= test_eth_newFilter(args.api)
    ret |= test_eth_newBlockFilter(args.api)
    ret |= test_eth_newPendingTransactionFilter(args.api)
    ret |= test_eth_uninstallFilter(args.api)
    ret |= test_eth_getFilterChanges(args.api)
    ret |= test_eth_getFilterLogs(args.api)
    ret |= test_eth_getLogs(args.api)
    ret |= test_eth_subscribe(args.api)
    ret |= test_eth_unsubscribe(args.api)
    ret |= test_eth_call(args.api)
    ret |= test_eth_estimateGas(args.api)
    ret |= test_eth_getTransactionCount(args.api)
    ret |= test_eth_getTransactionByHash(args.api)
    ret |= test_eth_getTransactionByBlockHashAndIndex(args.api)
    ret |= test_eth_getTransactionByBlockNumberAndIndex(args.api)
    ret |= test_eth_getTransactionReceipt(args.api)
    ret |= test_eth_sign(args.api)
    ret |= test_eth_signTransaction(args.api)
    ret |= test_eth_sendTransaction(args.api)
    ret |= test_eth_sendRawTransaction(args.api)

    if not ret:
        print(f"Test failed")
        sys.exit(1)
    else:
        print(f"Test passed!")


if __name__ == '__main__':
    main()
