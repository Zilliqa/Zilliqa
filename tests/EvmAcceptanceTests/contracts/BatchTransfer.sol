// SPDX-License-Identifier: MIT
pragma solidity ^0.8.9;

contract BatchTransfer {
    constructor() payable
    {
    }

    function batchTransfer(address payable[] memory destinations, uint256 amount) public
    {
        for (uint256 i=0; i<destinations.length; i++)
        {
            destinations[i].transfer(amount);
        }
        selfdestruct(payable(msg.sender));
    }
}