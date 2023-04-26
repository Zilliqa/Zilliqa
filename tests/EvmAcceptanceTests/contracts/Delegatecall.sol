// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

contract TestDelegatecall {
    address public sender;
    uint public value;
    uint public num;
    address public owner;
    mapping(address => address) public addresses;

   // function updateBalance(uint newBalance) public {
   //    balances[msg.sender] = newBalance;
   // }

    event DebugMsg(string message);
    event DebugMsgAddress(address message);

    function setVars(uint _num) external payable {

        require(owner == msg.sender, "only admin can call this contract");

        emit DebugMsg("Calling third level of delegate call");

        require(num == 3735928559, "Didn't reflect variables from calling contract correctly"); // 0xDEADBEEF

        sender = msg.sender;
        value = msg.value;
        num = _num;
    }

    function setOwnerAndCheck() external payable {

        emit DebugMsg("Checking first level of delegate call!");
        //require(owner == msg.sender, "only admin can call this contract");
        emit DebugMsg("Success.");

        addresses[msg.sender] = msg.sender;

        if (owner == msg.sender) {
            emit DebugMsg("Success!");
        } else {
            emit DebugMsg("Not succ!");
        }

        emit DebugMsgAddress(owner);
        emit DebugMsgAddress(msg.sender);
    }

    function OwnerOf(address id) public returns (address)  {

        emit DebugMsg("checking ID.");

        address toReturn = this.addresses(id);

        emit DebugMsgAddress(toReturn);
        return toReturn;
    }
}

contract Delegatecall {
    address public sender;
    uint public value;
    uint public num;
    address public owner;
    mapping(address => address) public addresses;

    event DebugMsg(string message);
    event DebugMsgAddress(address message);

    function setVars(address _test, uint _num) external payable {

        require(owner == msg.sender, "only admin can call this contract");

        emit DebugMsg("Calling second level of delegate call");

        num = 3735928559; // 0xDEADBEEF
        value = 4027445261; // 0xF00DF00D

        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setVars.selector, _num)
        );

        require(num == _num, "Didn't set it...");
        require(success, "delegatecall failed");
    }

    function setOwnerAndCheck(address _test) public returns (address) {

        require(owner == msg.sender, "only admin can call this contract");

        emit DebugMsg("Checking second level of delegate call");

        (bool success, bytes memory data) = _test.delegatecall(
            abi.encodeWithSelector(TestDelegatecall.setOwnerAndCheck.selector)
        );
    }

    function checkOther(address _first) public returns (address) {

        TestDelegatecall direct = TestDelegatecall(_first);
        address toRet = direct.OwnerOf(msg.sender);

        address toRetSecond = direct.addresses(msg.sender);


        emit DebugMsg("(level two): Here are first two return values: ");
        emit DebugMsgAddress(toRet);
        emit DebugMsgAddress(toRetSecond);
        return toRet;
    }
}

contract BaseDelegator {
    address public sender;
    uint public value;
    uint public num;
    address public owner;
    mapping(address => address) public addresses;

    event DebugMsg(string message);
    event DebugMsgAddress(address message);

    function setOwnerAndCheck(address _first, address _second) external payable {

        owner = msg.sender;

        emit DebugMsg("Checking first level of delegate call");
        emit DebugMsgAddress(owner);

        require(owner == msg.sender, "only admin can call this contract");

        (bool success, bytes memory data) = _first.delegatecall(
            abi.encodeWithSelector(Delegatecall.setOwnerAndCheck.selector, _second)
        );
    }

    // first is direct, second is via proxy
    function checkOwnerTwoWays(address _first, address _second) external payable {

        // first way, direcly call the contract
        TestDelegatecall direct = TestDelegatecall(_first);
        address toRet = direct.OwnerOf(msg.sender);

        address toRetSecond = direct.addresses(msg.sender);

        emit DebugMsg("Here are first two return values: ");
        emit DebugMsgAddress(toRet);
        emit DebugMsgAddress(toRetSecond);

        (bool success, bytes memory data) = _second.call(
            abi.encodeWithSelector(Delegatecall.checkOther.selector, _first)
        );
    }
}
