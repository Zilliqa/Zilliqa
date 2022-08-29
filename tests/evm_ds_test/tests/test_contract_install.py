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

import re
import time

import web3

from evm_ds_test import test_case
from evm_ds_test.utils import int_from_bytes
from evm_ds_test.utils import string_from_bytes
from evm_ds_test.utils import process_data
from evm_ds_test.utils import process_log


class TestContractInstall(test_case.EvmDsTestCase):

    contract = """
// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

/**
 * @title Storage
 * @dev Store & retrieve value in a variable
 */
contract Storage {

    uint256 number = 1024;
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

    contract2 = """
    // SPDX-License-Identifier: MIT
pragma solidity ^0.8.13;

// Base contract X
contract X {
    string name;

    constructor(string memory _name) {
        name = _name;
    }
}
"""

    contract3 = """
pragma solidity ^0.8.13;

contract Event {
    // Event declaration
    // Up to 3 parameters can be indexed.
    // Indexed parameters helps you filter the logs by the indexed parameter
    event Log(address indexed sender, string message);
    event AnotherLog();

    function test() public {
        emit Log(msg.sender, "Hello World!");
        emit Log(msg.sender, "Hello EVM!");
        emit AnotherLog();
    }
}
"""

    contract4 = """
contract Test { 
    function double(int a) public view returns(int) {
        return 2*a;
    } 
}
"""

    def test_contract_install_no_constructor(self):
        self.init()
        compilation_result = self.compile_solidity(self.contract)
        contract = self.install_contract(self.account, compilation_result)
        time.sleep(2)
        # Check correctness of the contract installation.
        result = self.call_contract(self.account, contract, 0, "store", 256)
        time.sleep(2)
        assert int_from_bytes(self.get_storage_at(contract.address, 0)) == 256

    def test_contract_install_with_constructor(self):
        self.init()
        name_string = "Kaustubh"
        compilation_result = self.compile_solidity(self.contract2)
        bytes_with_constructor_params = compilation_result.constructor(
            name_string
        ).data_in_transaction
        contract = self.install_contract_bytes(
            self.account, bytes_with_constructor_params
        )
        assert (
            string_from_bytes(self.get_storage_at(contract.address, 0)) == name_string
        )

    # For testing events, look at the receipt in the txn
    # Incomplete for now, add assert
    def disabled_test_events(self):
        self.init()
        compilation_result = self.compile_solidity(self.contract3)
        contract = self.install_contract(self.account, compilation_result)
        time.sleep(2)
        result = self.call_contract(self.account, contract, 0, "test")
        logs = result["receipt"]["event_logs"]
        assert (
            logs[0]["topics"][0]
            == "0x0738f4da267a110d810e6e89fc59e46be6de0c37b1d5cd559b267dc3688e74e0"
        )
        assert (
            logs[0]["topics"][1]
            == "0x000000000000000000000000" + self.account.address.lower()
        )
        ret_hex = process_data(logs[1]["data"])
        ret = process_log(logs[1], 1, result["ID"], result["receipt"]["epoch_num"])
        processed_log = contract.events.Log().processLog(ret)
        assert (
            ret_hex
            == "0000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000000a48656c6c6f2045564d2100000000000000000000000000000000000000000000"
        )
        assert processed_log["args"]["message"] == "Hello EVM!"

    def test_contract_eth_call(self):
        self.init()
        compilation_result = self.compile_solidity(self.contract4)
        contract = self.install_contract(self.account, compilation_result)
        time.sleep(2)
        resp = self.call_view(contract, "double", 20)
        # Function should double the param value
        assert resp == 40
