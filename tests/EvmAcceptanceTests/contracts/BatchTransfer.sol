// SPDX-License-Identifier: MIT
pragma solidity ^0.8.9;

contract BatchTransfer {
    event DebugMe(string);

    constructor() payable
    {
    }

    function batchTransfer(address payable[] memory destinations, uint256 amount) public
    {
        //for (uint256 i=0; i<destinations.length-1; i++)
        //{
        //    emit DebugMe("Make the transfer!!!");
        //    destinations[i].transfer(amount);
        //}
        emit DebugMe("Make the transfer!!!");
        destinations[0].transfer(amount);

        emit DebugMe("about to self destruct!!!");
        selfdestruct(payable(msg.sender));
    }
}
