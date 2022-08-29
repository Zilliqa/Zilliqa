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

from pyzil.account import Account
from evm_ds_test import test_case, from_zil

import solcx
from web3 import Web3
from eth_utils import to_checksum_address as eth_to_checksum_address


class TestParentChild(test_case.EvmDsTestCase):

    contract = """
// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract ChildContract {
    uint256 data;
    uint256 public value;
    address payable public sender;

    constructor(uint256 _data) payable {
       data = _data;
       value = msg.value;
       sender = payable(msg.sender);
    }

    function read() public view returns (uint256) {
       return data;
    }

    function returnToSender() public {
       uint amount = address(this).balance;
       (bool success, ) = sender.call{value: amount}("");
       require(success, "Failed to send Ether");
    }
}

contract ParentContract {
    
    ChildContract public child;
    uint256 public value;

    constructor () payable {
      value = msg.value;
    }

    function installChild(uint256 initial_data) public returns (address payable) {
      child = new ChildContract{value: 300000 gwei}(initial_data);
      return payable(address(child));
    }

    function childAddress() public view returns (address payable) {
      return payable(address(child));
    }

    function getPaidValue() public view returns (uint256) {
      return value;
    }

    receive() external payable {
    }
}

"""

    def test_installing_child_by_calling_parent_public_function(self):
        self.init()
        # At first, compile and install the parent contract.
        compilation_result = self.compile_solidity(self.contract)
        contract = self.install_contract(
            self.account, compilation_result, value=from_zil(300)
        )

        # Record the initial balance.
        prev_balance = self.get_balance(self.account.address)

        # Check that the parent contract account is 300 from_zil as initially requested.
        set_value = self.call_view(contract, "getPaidValue")
        assert set_value == 300_000_000_000_000

        assert self.get_balance(contract.address) == from_zil(300)

        # Install a child contract by calling the method of a parent contract.
        # Supply the initial data argument to verify later.
        self.call_contract(self.account, contract, 0, "installChild", 12345)
        child_contract_addr = self.call_view(contract, "childAddress")

        # Check that the child received 300 from_zil (like written in parent Solidity)
        assert self.get_balance(child_contract_addr) == from_zil(300)

        # Need to select the right contract, so have to recompile it again here.
        result = solcx.compile_source(self.contract, output_values=["abi"])
        child_contract = self.w3.eth.contract(
            address=eth_to_checksum_address(child_contract_addr),
            abi=result["<stdin>:ChildContract"]["abi"],
        )

        # Check that the initial data we initiated the child contract with matches.
        data = self.call_view(child_contract, "read")
        assert data == 12345

        # Check that the recorded sender is the same as the parent contract.
        sender = self.call_view(child_contract, "sender")
        assert eth_to_checksum_address(sender) == eth_to_checksum_address(
            contract.address
        )

        # Return all funds from the child to its sender - parent contract.
        self.call_contract(self.account, child_contract, 0, "returnToSender")
        assert self.get_balance(child_contract_addr) == 0
        assert self.get_balance(contract.address) == from_zil(300)
