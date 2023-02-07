// SPDX-License-Identifier: MIT
pragma solidity ^0.8.9;

contract SingleTransfer {

    event DebugMessage(string message);
    event DebugMessageExtended(uint256 amount, address indexed addr, string message);

    constructor() payable
    {
    }

    function doTransfer(address payable destination, uint256 amount) public payable
    {
        emit DebugMessageExtended(amount, destination, "sending...");

        (bool success1, ) = destination.call{value: amount}("");

        require(success1, "Unable to send amount");
        emit DebugMessage("Done.");
    }
}