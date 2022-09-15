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
import os
import time

import numpy as np
import pkg_resources
import solcx
from eth_account import Account as EthAccount
from eth_utils import to_checksum_address as eth_to_checksum_address
from fastecdsa import curve, keys
from hexbytes import HexBytes
from web3 import Web3
from web3.exceptions import TransactionNotFound

from evm_ds_test.utils import from_gwei, from_zil


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
    return b"\x00" * padding + b


class EvmDsTestCase:
    """
    Base class for all EVM DS test cases.
    """

    endpoint = os.environ.get("ZILLIQA_API", "http://localhost:5555")
    network_id = int(os.environ.get("ZILLIQA_CHAIN_ID", "1"), 0)
    eth_network_id = network_id + 0x8000

    def init(self, num_accounts=1):
        self.w3 = Web3(Web3.HTTPProvider(self.endpoint, request_kwargs={'timeout': 90}))
        funded_account = EthAccount.from_key(
            private_key="d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba"
        )
        self.accounts = [
            EthAccount.from_key(
                pad_uint256(
                    keys.gen_private_key(curve.secp256k1, randfunc=np.random.bytes)
                )
            )
            for _ in range(num_accounts)
        ]
        self.account = self.accounts[0]

        nonce = self.get_nonce(funded_account.address)

        logging.debug("Funding initial test accounts")
        # Fund all the accounts.
        pending_transactions = []
        for account in self.accounts:
            amount = from_zil(200)
            logging.info(
                "Funding account {} with {} Qa".format(account.address, amount)
            )
            txn_id = self.transfer_zil(funded_account, account, amount, nonce)
            pending_transactions.append(txn_id)
            nonce += 1

        # Wait for all initial funding transactions to complete.
        start = time.time()
        while time.time() - start <= 240 and pending_transactions:
            txn_id = pending_transactions.pop(0)
            try:
                txn = self.w3.eth.get_transaction(txn_id)
                if txn and "hash" in txn:
                    return txn["hash"]
            except TransactionNotFound:
                pending_transactions.append(txn_id)
                time.sleep(1.0)
        if pending_transactions:
            raise RuntimeError("Failed to complete initial payment transactions")

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

    def transfer_zil(self, source, dest, amount, nonce=None):
        if nonce is None:
            nonce = self.get_nonce(source.address)
        signed_txn = self.w3.eth.account.sign_transaction(
            {
                "to": dest.address,
                "value": amount,
                "gas": 25_000,
                "gasPrice": from_gwei(10_000),
                "nonce": nonce,
                "chainId": self.eth_network_id,
                "data": b"",
            },
            source.key,
        )
        return self.w3.eth.send_raw_transaction(signed_txn.rawTransaction).hex()

    def install_contract(self, account, contract_class, *args, **kwargs):
        """
        Send a Zilliqa transaction that creates a given contract.

        Returns: contract address.
        """
        value = kwargs.get("value", 0)
        confirm = kwargs.get("confirm", True)
        balance = self.get_balance(account.address)
        nonce = self.get_nonce(account.address)
        logging.debug(
            "Account {} balance {}, nonce {}".format(account.address, balance, nonce)
        )
        txn = contract_class.constructor(*args).build_transaction(
            {
                "gas": 295_000,
                "gasPrice": from_gwei(10_000),
                "value": value,
                "nonce": nonce,
                "chainId": self.eth_network_id,
            }
        )
        signed_txn = self.w3.eth.account.sign_transaction(txn, account.key)
        logging.info(
            "Balance of sending account is {}".format(self.get_balance(account.address))
        )
        logging.info("Installing contract from account {}".format(account.address))
        install_txn = self.w3.eth.send_raw_transaction(signed_txn.rawTransaction).hex()
        if confirm:
            receipt = self.wait_for_transaction_receipt(install_txn)
            if receipt:
                address = receipt.get("contractAddress", None)
                logging.debug("deployed contract at address: {}".format(address))
                return contract_class(address=address)
            else:
                raise RuntimeError("Could not wait for txn ID {}".format(install_txn))

    def install_contract_bytes(self, account, data_bytes, *args, **kwargs):
        confirm = kwargs.get("confirm", True)
        value = kwargs.get("value", 0)
        nonce = self.get_nonce(account.address)
        txn = self.w3.eth.account.sign_transaction(
            {
                "value": value,
                "data": data_bytes,
                "gas": 85_000,
                "gasPrice": from_gwei(10_000),
                "nonce": nonce,
                "chainId": self.eth_network_id,
            },
            account.key,
        )
        install_txn = self.w3.eth.send_raw_transaction(
            Web3.toHex(txn.rawTransaction)
        ).hex()
        if confirm:
            receipt = self.wait_for_transaction_receipt(install_txn)
            if receipt:
                address = receipt.get("contractAddress", None)
                logging.debug("deployed contract at address: {}".format(address))
                return self.w3.eth.contract(address=address)
            else:
                raise RuntimeError("Could not wait for txn ID {}".format(install_txn))

    def wait_for_transaction_receipt(self, txn_id, timeout=300):
        start = time.time()
        while time.time() - start <= timeout:
            try:
                return self.w3.eth.get_transaction_receipt(txn_id)
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
        nonce = self.get_nonce(account.address)
        balance = self.get_balance(account.address)
        logging.debug(
            "Account {} balance {}, nonce {}".format(account.address, balance, nonce)
        )
        txn = function(*args, **kwargs).build_transaction(
            {
                "from": account.address,
                "value": value,
                "gas": 295_000,
                "gasPrice": from_gwei(10_000),
                "nonce": nonce,
                "chainId": self.eth_network_id,
            }
        )
        signed_txn = self.w3.eth.account.sign_transaction(txn, account.key)
        call_txn = self.w3.eth.send_raw_transaction(signed_txn.rawTransaction).hex()
        logging.info("call txn {}".format(call_txn))
        if confirm:
            txn_id = self.wait_for_transaction_receipt(call_txn)
            if txn_id and "transactionHash" in txn_id:
                return txn_id.get("transactionHash", None)
            else:
                return None

    def get_storage_at(self, address, position):
        ret = bytes(
            self.w3.eth.get_storage_at(
                pad_address(address), pad_uint256(position).hex()
            )
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
                "gasPrice": from_gwei(10_000),
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
