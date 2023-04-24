// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

contract TestDelegatecall {
    address public sender;
    uint public value;
    uint public num;

    function setVars(uint _num) external payable {

        require(num == 3735928559, "Didn't reflect variables from calling contract correctly"); // 0xDEADBEEF

        sender = msg.sender;
        value = msg.value;
        num = _num;
    }
}

contract Delegatecall {
    address public sender;
    uint public value;
    uint public num;

    function setVars(address _test, uint _num) external payable {

        num = 3735928559; // 0xDEADBEEF
        value = 4027445261; // 0xF00DF00D

        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setVars.selector, _num)
        );

        require(num == _num, "Didn't set it...");
        require(success, "delegatecall failed");
    }
}
