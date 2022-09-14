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

from evm_ds_test import test_case, from_zil
from eth_account import Account as EthAccount

import time


class TestTransferZil(test_case.EvmDsTestCase):

    contract = """
// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract ForwardZil {

    // Payable address can receive Zil.
    address payable public owner;

    // Payable constructor can receive Ether
    constructor() payable {
        owner = payable(msg.sender);
    }

    function deposit() public payable {}

    // Call this function along with some Zil
    // The function will throw an error since this function is not payable.
    function notPayable() public {}

    // Function to withdraw all Ether from this contract.
    function withdraw() public {
        // get the amount of Zil stored in this contract minus 10 zil.
        uint amount = address(this).balance;
        if (amount > 1 ether) {  // 1 ZIL
            amount -= 1 ether;
        }

        // send all Ether to owner
        // Owner can receive Ether since the address of owner is payable
        (bool success, ) = owner.call{value: amount}("");
        require(success, "Failed to send Ether");
    }

    // Function to transfer Ether from this contract to address from input
    function transfer(address payable _to, uint _amount) public {
        // Note that "to" is declared as payable
        (bool success, ) = _to.call{value: _amount}("");
        require(success, "Failed to send Ether");
    }
}
"""

    def test_zil_payments(self):
        """Check whether we can send ZILs to contract accounts with EVM code correctly."""
        self.init(num_accounts=3)

        compilation_result = self.compile_solidity(self.contract)
        contract = self.install_contract(self.account, compilation_result)

        # Record the initial balance.
        prev_balance = self.get_balance(self.account.address)

        # Check that we can pass apparent value to a payable method in a contract
        # and it is accepted. In EVM, the payment is accepted as long as there's
        # no revert.
        assert self.get_balance(contract.address) == 0
        self.call_contract(self.account, contract, from_zil(2), "deposit")
        assert self.get_balance(contract.address) == from_zil(2)

        # Check that we can pass apparent value to a non-payable method. It should revert
        # as the passed value is not zero, so the resulting balance should not change.
        try:
            self.call_contract(
                self.account, contract, from_zil(1), "notPayable", confirm=False
            )
        except ValueError:
            pass  # For the isolated server
        self.wait_txn_timeout()
        assert self.get_balance(contract.address) == from_zil(2)  # Should not change.

        # Check that we can have the contract itself send some value to some address.
        # We expect the resulting balance to be zero, as the contract sends all of
        # its ZILs back.
        self.call_contract(self.account, contract, 0, "withdraw")
        time.sleep(1)

        assert self.get_balance(contract.address) == from_zil(1)

        # Now we should get 1 Zil back to account. Minus the gas charges.
        # But overall we should be down 1 Zil + any gas fees
        account_diff = prev_balance - self.get_balance(self.account.address)
        assert account_diff > from_zil(1)
        assert account_diff < from_zil(10)  # Gas fees could still be high sometimes!
