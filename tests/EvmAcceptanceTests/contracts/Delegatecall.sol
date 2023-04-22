// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

contract TestDelegatecall {
    address public sender;
    uint public value;
    uint public num;

    event A(string message);

    function setVars(uint _num) external payable {

        emit A("we are here 0");
        require(value == 9, "Didn't reflect variables from calling contract correctly");
        emit A("we are here 1");
        require(num == 10, "Didn't reflect variables from calling contract correctly");
        emit A("we are here 2");

        sender = msg.sender;
        value = msg.value;
        num = _num;
    }
}

contract Delegatecall {
    address public sender;
    uint public value;
    uint public num;

    event A(string message);

    function setVars(address _test, uint _num) external payable {

        value = 9;
        num = 10;

        require(value == 9, "cannot set own variables correctly (value)");
        require(num == 10, "cannot set own variables correctly(num)");


        emit A("making the call...");

        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setVars.selector, _num)
        );

        emit A("making the call... done.");

        if (success) {
            emit A("this was a success");
        } else {
            emit A("this was a failure");
        }

        require(success, "delegatecall failed");
    }
}
