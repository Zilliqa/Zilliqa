// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

contract TestDelegatecall {
    address public sender;
    uint public value;
    uint public num;

    event A(string message);

    function setVars(uint _num) external payable {

        emit A("we are here 0");
        require(num == 3735928559, "Didn't reflect variables from calling contract correctly"); // 0xDEADBEEF

        emit A("we are here 1");

        sender = msg.sender;
        value = msg.value;
        num = _num;
        emit A("we are here 3");
    }
}

contract Delegatecall {
    address public sender;
    uint public value;
    uint public num;

    event A(string message);

    function setVars(address _test, uint _num) external payable {

        num = 3735928559; // 0xDEADBEEF
        value = 4027445261; // 0xF00DF00D

        emit A("making the call...");

        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setVars.selector, _num)
        );

        emit A("making the call... done.");

        require(num == _num, "Didn't set it...");

        if (success) {
            emit A("this was a success");
        } else {
            emit A("this was a failure");
        }

        require(success, "delegatecall failed");
    }
}
