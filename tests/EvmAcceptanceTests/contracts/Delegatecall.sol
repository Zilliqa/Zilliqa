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

    function test() external payable {
    }

    function setVars(uint _num) external payable {

        require(owner == msg.sender, "only admin can call this contract");

        emit DebugMsg("Calling third level of delegate call");

        require(num == 3735928559, "Didn't reflect variables from calling contract correctly"); // 0xDEADBEEF

        sender = msg.sender;
        value = msg.value;
        num = _num;
    }

    function ownerSigh() public returns (address)  {
        address easy = tx.origin;
        address toSend = addresses[easy];

        emit DebugMsg("Whee we return");
        emit DebugMsgAddress(toSend);
        return toSend;
    }

    function setOwnerAndCheck() external payable {

        emit DebugMsg("Checking first level of delegate call!");
        //require(owner == msg.sender, "only admin can call this contract");
        emit DebugMsg("Success.");

        address easy = tx.origin;
        addresses[easy] = msg.sender;
        owner = msg.sender;

        if (owner == msg.sender) {
            emit DebugMsg("Success!");
        } else {
            emit DebugMsg("Not succ!");
        }

        emit DebugMsgAddress(msg.sender);

        emit DebugMsg("OUR RESULT: ");
        emit DebugMsgAddress(ownerSigh());
    }

    function OwnerOf(address id) public returns (address)  {

        emit DebugMsg("checking ID.");

        address easy = tx.origin;
        address toReturn = this.addresses(easy);

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

    function foo() external payable {
    }

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
        //address toRet = direct.OwnerOf(msg.sender);

        address easy = tx.origin;
        address toRetSecond = direct.addresses(easy);
        //address toRetThird = direct.ownerSigh();


        emit DebugMsg("(level two): Here are first two return values: ");
        //emit DebugMsgAddress(_first);
        //emit DebugMsgAddress(toRet);
        emit DebugMsgAddress(toRetSecond);
        emit DebugMsgAddress(tx.origin);
        //emit DebugMsgAddress(toRetThird);
        emit DebugMsg("(level two): We could actualy break here... ");
        return msg.sender;
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

    function bar() external payable {
    }

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
        //address toRet = direct.OwnerOf(msg.sender);

        address easy = tx.origin;
        address toRetSecond = direct.addresses(easy);
        //address toRetThird = direct.ownerSigh();

        emit DebugMsg("Here are first two return values: ");
        //emit DebugMsgAddress(toRet);
        emit DebugMsgAddress(toRetSecond);
        emit DebugMsgAddress(tx.origin);
        //emit DebugMsgAddress(toRetThird);

        (bool success, bytes memory data) = _second.delegatecall(
            abi.encodeWithSelector(Delegatecall.checkOther.selector, _first)
        );
    }
}
