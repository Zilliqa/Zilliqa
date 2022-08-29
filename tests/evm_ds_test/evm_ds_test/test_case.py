#
# Copyright (C) 2022 Zilliqa
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

import logging
import pkg_resources

import os
import time
from eth_abi import decode_abi
from eth_utils import decode_hex, to_checksum_address as eth_to_checksum_address
from eth_account import Account as EthAccount
from hexbytes import HexBytes
from fastecdsa import keys, curve
import numpy as np

from pyzil.zilliqa.api import APIError
from pyzil.zilliqa.chain import BlockChain, set_active_chain, active_chain
from pyzil.account import Account
from pyzil.crypto.zilkey import to_checksum_address, ZilKey
import solcx

from web3 import Web3
from web3._utils.abi import get_constructor_abi, get_abi_output_types
from web3._utils.contracts import encode_abi, get_function_info
from web3.exceptions import TransactionNotFound
from .utils import int_from_bytes


def pad_address(address):
    b = bytes.fromhex(address.replace("0x", ""))
    padding = 20 - len(b)
    if padding < 0:
        padding = 0
    return eth_to_checksum_address((b"\x00" * padding + b).hex())


def pad_uint256(value):
    if isinstance(value, int):
        value = "%064x" % value
    b = bytes.fromhex(value.replace("0x", ""))
    padding = 32 - len(b)
    if padding < 0:
        padding = 0
    return (b"\x00" * padding + b)


class EvmDsTestCase:
    """
    Base class for all EVM DS test cases.
    """

    endpoint = os.environ.get("ZILLIQA_API", "http://localhost:5555")
    network_id = int(os.environ.get("ZILLIQA_CHAIN_ID", "1"), 0)
    eth_network_id = network_id + 0x8000

    def init(self, num_accounts=1):
        version = int("0x%04x%04x" % (self.network_id, 1), 0)
        self.blockchain = BlockChain(
            api_url=self.endpoint, version=version, network_id=self.network_id
        )
        set_active_chain(self.blockchain)
        funded_account = Account(
            private_key="d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba"
        )
        self.accounts = [
            EthAccount.from_key(
                pad_uint256(keys.gen_private_key(curve.secp256k1, randfunc=np.random.bytes))
            )
            for _ in range(num_accounts)
        ]
        self.account = self.accounts[0]

        nonce = funded_account.get_nonce() + 1

        logging.debug("Funding initial test accounts")
        # Fund all three accounts.
        pending_transactions = []
        for account in self.accounts:
            txn_details = funded_account.transfer(
                to_addr=to_checksum_address(account.address),
                zils=1000,
                gas_limit=100,
                priority=True,
                nonce=nonce,
                confirm=False,
            )
            logging.debug("TXN Details: '{}'".format(txn_details))
            pending_transactions.append(txn_details["TranID"])
            nonce += 1
        # Wait for all initial funding transactions to complete.
        start = time.time()
        time.sleep(1)
        while time.time() - start <= 240 and pending_transactions:
            txn_id = pending_transactions.pop(0)
            try:
                active_chain.api.GetTransaction(txn_id)
            except APIError as e:
                logging.debug("Retry GetTransaction: {}".format(e))
                pending_transactions.append(txn_id)
                time.sleep(1.0)
        if pending_transactions:
            raise RuntimeError("Failed to complete initial payment transactions")

        self.w3 = Web3(Web3.HTTPProvider(self.endpoint))

    def compile_solidity(self, contract_string):
        result = solcx.compile_source(
            contract_string, output_values=["abi", "bin", "asm"]
        )
        result = result.popitem()[1]
        return self.w3.eth.contract(abi=result["abi"], bytecode=result["bin"])

    def compile_solidity_file(self, contract_file):
        file_name = pkg_resources.resource_filename(
            "tests", "contracts/" + contract_file
        )
        result = solcx.compile_files(file_name, output_values=["abi", "bin", "asm"])
        result = result.popitem()[1]
        return self.w3.eth.contract(abi=result["abi"], bytecode=result["bin"])

    def install_contract(self, account, contract_class, *args, **kwargs):
        """
        Send a Zilliqa transaction that creates a given contract.

        Returns: contract address.
        """
        value = kwargs.get("value", 0)
        confirm = kwargs.get("confirm", True)
        nonce = self.w3.eth.get_transaction_count(account.address)
        txn = contract_class.constructor(*args).build_transaction(
            {
                "from": account.address,
                "gas": 30_000,
                "gasPrice": 2_000_000_000,
                "value": value,
                "nonce": nonce,
                "chainId": self.eth_network_id,
            }
        )
        signed_txn = self.w3.eth.account.sign_transaction(txn, account.key)
        install_txn = self.w3.eth.send_raw_transaction(signed_txn.rawTransaction).hex()
        if confirm:
            txn_id = self.wait_for_transaction_receipt(install_txn).hex()
            logging.info("done waiting for txn_id {}".format(txn_id))
            if txn_id:
                txn_id = txn_id.replace("0x", "")
                address = Web3.toChecksumAddress(
                    # TODO: remove dependence on this Zilliqa API. Use TXN receipt instead.
                    active_chain.api.GetContractAddressFromTransactionID(txn_id)
                )
                return contract_class(address=address)

    def install_contract_bytes(self, account, data_bytes, *args, **kwargs):
        confirm = kwargs.get("confirm", True)
        value = kwargs.get("value", 0)
        nonce = self.w3.eth.get_transaction_count(account.address)
        txn = self.w3.eth.account.sign_transaction(
            {
                "from": account.address,
                "value": value,
                "data": data_bytes,
                "gas": 30_000,
                "gasPrice": 2_000_000_000,
                "nonce": nonce,
                "chainId": self.eth_network_id,
            },
            account.key,
        )
        install_txn = self.w3.eth.send_raw_transaction(
            Web3.toHex(txn.rawTransaction)
        ).hex()
        if confirm:
            txn_id = self.wait_for_transaction_receipt(install_txn).hex()
            logging.info("done waiting for txn_id {}".format(txn_id))
            if txn_id:
                txn_id = txn_id.replace("0x", "")
                address = Web3.toChecksumAddress(
                    active_chain.api.GetContractAddressFromTransactionID(txn_id)
                )
                return self.w3.eth.contract(address=address)

    def wait_for_transaction_receipt(self, txn_id, timeout=300):
        start = time.time()
        while time.time() - start <= timeout:
            try:
                txn = self.w3.eth.get_transaction(txn_id)
                if txn and "hash" in txn:
                    return txn["hash"]
            except TransactionNotFound:
                pass
            logging.debug("Waiting for transaction: " + txn_id)
            time.sleep(5)
        return None

    def call_contract(self, account, contract, value, method, *args, **kwargs):
        """
        Call the contract's method with arguments as transaction.
        """
        # Use contract ABI to encode arguments.
        confirm = kwargs.get("confirm", True)
        if "confirm" in kwargs:
            del kwargs["confirm"]
        function = contract.get_function_by_name(method)
        nonce = self.w3.eth.get_transaction_count(account.address)
        txn = function(*args, **kwargs).build_transaction(
            {
                "from": account.address,
                "value": value,
                "gas": 30_000,
                "gasPrice": 2_000_000_000,
                "nonce": nonce,
                "chainId": self.eth_network_id,
            }
        )
        signed_txn = self.w3.eth.account.sign_transaction(txn, account.key)
        call_txn = self.w3.eth.send_raw_transaction(signed_txn.rawTransaction).hex()
        logging.info("call txn {}".format(call_txn))
        if confirm:
            txn_id = self.wait_for_transaction_receipt(call_txn).hex()
            return txn_id

    def get_storage_at(self, address, position):
        ret = bytes(
            self.w3.eth.get_storage_at(pad_address(address), pad_uint256(position).hex())
        )
        padding = 32 - len(ret)
        if padding < 0:
            padding = 0
        return HexBytes(b"\x00" * padding + ret)

    def call_view(self, contract, method, *args, **kwargs):
        function = contract.get_function_by_name(method)
        result = function(*args, **kwargs).call(
            {
                "from": self.account.address,
                # "gas": 30_000,
                "gasPrice": 2_000_000_000,
                "chainId": self.eth_network_id,
            }
        )
        return result

    def wait_txn_timeout(self):
        # Wait until the transaction has taken effect.
        # On the isolated server it is likely instant.
        # On the devnet/testnet we'll wait for 4 minutes
        if "localhost" in self.endpoint:
            time.sleep(0.2)
        else:
            time.sleep(240)

    def get_balance(self, address):
        return self.w3.eth.get_balance(address)

    def get_nonce(self, address):
        return self.w3.eth.get_transaction_count(address)
