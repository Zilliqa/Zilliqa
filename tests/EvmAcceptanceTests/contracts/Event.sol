// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract Event {
    // Event declaration
    // Up to 3 parameters can be indexed.
    // Indexed parameters helps you filter the logs by the indexed parameter
    event Log(address indexed sender, string message);
    event AnotherLog();

    function one_log() public {
        emit Log(msg.sender, "Hello World!");
    }

    function two_logs() public {
        emit Log(msg.sender, "Hello World!");
        emit AnotherLog();
    }
}