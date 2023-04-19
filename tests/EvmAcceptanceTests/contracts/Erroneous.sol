// SPDX-License-Identifier: GPL-3.0-or-later

pragma solidity ^0.8.7;

contract Erroneous {
    function foo() external returns (bool) {
        // Allocate lot's of memory so gas usage is high
        bytes memory chunk = new bytes(4096);

        for(uint256 i=0; i < chunk.length; i++) {
            chunk[i] = 0xFF;
            bytes memory result = abi.encode("Foo", "Bar", "Some");
        }
        require(true == false);
        return true;
    }
}
