// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

contract TestDelegatecall {
    address public sender;
    uint public value;
    uint public num;

    function setVars(uint _num) external payable {
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
        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setVars.selector, _num)
        );

        require(success, "delegatecall failed");
    }
}