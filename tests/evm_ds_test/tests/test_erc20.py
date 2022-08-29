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

import time
import pytest
from eth_utils import decode_hex, to_checksum_address as eth_to_checksum_address

from evm_ds_test import test_case


class TestERC20(test_case.EvmDsTestCase):
    @pytest.fixture(autouse=True)
    def _deploy_erc20(self):
        self.init(num_accounts=4)
        compilation_result = self.compile_solidity_file("ERC20.sol")
        self.contract = self.install_contract(
            self.account, compilation_result, "ZERC", "Zillia ERC Token", 100_000
        )

    def test_token_basics(self):
        assert self.call_view(self.contract, "totalSupply") == 100_000
        assert self.call_view(self.contract, "symbol") == "ZERC"
        assert (
            self.call_view(self.contract, "balanceOf", self.account.address) == 100_000
        )
        assert (
            self.call_view(
                self.contract,
                "allowance",
                self.account.address,
                eth_to_checksum_address("0x0000000000000000000000000000000000000000"),
            )
            == 0
        )

    def test_transfer(self):
        """Check that you can transfer balances of between addresses."""
        prev_balance = self.call_view(self.contract, "balanceOf", self.account.address)
        self.call_contract(
            self.account,
            self.contract,
            0,
            "transfer",
            self.accounts[1].address,
            1000,
        )
        # TODO: check emitted event.
        assert (
            self.call_view(self.contract, "balanceOf", self.accounts[1].address) == 1000
        )
        assert self.call_view(self.contract, "balanceOf", self.account.address) == (
            prev_balance - 1000
        )

        # Have already spent 1000, shouldn't work
        # TODO: revert to testing failed transaction after we have the possibility
        # with pytest.raises(APIError) as e:
        try:
            self.call_contract(
                self.account,
                self.contract,
                0,
                "transfer",
                self.accounts[2].address,
                prev_balance,
                confirm=False,
            )
        except ValueError:
            pass  # For testing on the isolated server.
        self.wait_txn_timeout()
        assert self.call_view(self.contract, "balanceOf", self.account.address) == (
            prev_balance - 1000
        )

    def test_approve(self):
        """Check that the ERC-20 approve method sets approval information in the contract."""
        self.call_contract(
            self.accounts[1],
            self.contract,
            0,
            "approve",
            self.accounts[2].address,
            50_000,
        )
        assert (
            self.call_view(
                self.contract,
                "allowance",
                self.accounts[1].address,
                self.accounts[2].address,
            )
            == 50_000
        )

    def test_transfer_from(self):
        # Fund the 2nd account first
        self.call_contract(
            self.account,
            self.contract,
            0,
            "transfer",
            self.accounts[2].address,
            50_000,
        )
        # Approve 30_000 for withdrawal by Acc #3
        self.call_contract(
            self.accounts[2],
            self.contract,
            0,
            "approve",
            self.accounts[3].address,
            30_000,
        )
        # Try transferring allowed amount.
        self.call_contract(
            self.accounts[3],
            self.contract,
            0,
            "transferFrom",
            self.accounts[2].address,
            self.accounts[3].address,
            29_999,
        )
        # Check that the allowed amount is transferred okay
        assert (
            self.call_view(
                self.contract,
                "balanceOf",
                self.accounts[2].address,
            )
            == 20_001
        )
        # Check that the allowance was updated.
        assert (
            self.call_view(
                self.contract,
                "allowance",
                self.accounts[2].address,
                self.accounts[3].address,
            )
            == 1
        )
