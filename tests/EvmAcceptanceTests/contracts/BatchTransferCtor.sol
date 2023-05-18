// SPDX-License-Identifier: MIT
pragma solidity ^0.8.9;

contract BatchTransferCtor {

    event DebugMe(string);

    constructor(address payable[] memory destinations, uint256 amount) payable
    {
        //for (uint256 i=0; i<destinations.length -1; i++)
        //{
        //    destinations[i].transfer(amount);
        //}

        emit DebugMe("Make the transfer 000!!!");
        destinations[0].transfer(amount);
        emit DebugMe("Make the transfer 001!!!");
        destinations[1].transfer(amount);

        emit DebugMe("self destruct and return the transfer 000!!!");
        selfdestruct(payable(msg.sender));
    }
}
