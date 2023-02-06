// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract ContractOne {

    event DebugMessage(uint index, string message);

    constructor() payable
    {
        emit DebugMessage(0, "Contract one constructor...");
    }

    function chainedCallTemp(address []memory destinations) public
    {
        emit DebugMessage(0, "Contract one temp call......");
    }


    function chainedCallTempEmpty() public
    {
        emit DebugMessage(0, "Contract one XXX call......");
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {
        emit DebugMessage(index, "Chained call of contract one!");

        if (destinations.length > index) {
            ContractTwo two = ContractTwo(destinations[index]);

            emit DebugMessage(index, "Chained call of contract thing thing...!");

            (bool success) = two.chainedCall(destinations, index + 1);
            //emit Response(success, data);
        } {
            emit DebugMessage(index, "Reached end of array.");
        }

        emit DebugMessage(index, "Chained call of contract one fin!");

        //emit Response(success, data);
        //(bool success, bytes memory data) = destinations[index].call(
        //    abi.encodeWithSignature("chainedCall(string,uint256)", "call chainedCall", 123)
        //);
    }

    //function foo(string memory _message, uint _x) public payable returns (uint) {
    //    emit Received(msg.sender, msg.value, _message);

    //    return _x + 1;
    //}
}


contract ContractTwo {

    event DebugMessage(uint index, string message);

    constructor() payable
    {
        emit DebugMessage(0, "Contract two constructor...");
    }

    //fallback() external payable {
    //    emit Received(msg.sender, msg.value, "Fallback was called");
    //}

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {
        emit DebugMessage(index, "Chained call of contract two!");


        // Contract two will call three TWICE
        if (destinations.length > index) {
            ContractThree three = ContractThree(destinations[index]);
            emit DebugMessage(index, "Chained call of contract two - constructed...!");

            emit DebugMessage(index, "Chained call of contract two - 0...!");
            three.chainedCall(destinations, index + 1);
            emit DebugMessage(index, "Chained call of contract two - 1...!");
            (bool success) = three.chainedCall(destinations, index + 1);
            emit DebugMessage(index, "Chained call of contract two - 2...!");
            //emit Response(success, data);
        } {
            emit DebugMessage(index, "Reached end of array.");
        }

        emit DebugMessage(index, "Chained call of contract two fin!");

        //(bool success, bytes memory data) = destinations[index].call(
        //    abi.encodeWithSignature("chainedCall(string,uint256)", "call chainedCall", 123)
        //);
    }


    //function foo(string memory _message, uint _x) public payable returns (uint) {
    //    emit Received(msg.sender, msg.value, _message);

    //    return _x + 1;
    //}
}

contract ContractThree {

    event DebugMessage(uint index, string message);

    constructor() payable
    {
        emit DebugMessage(0, "Contract three constructor...");
    }

    //fallback() external payable {
    //    emit Received(msg.sender, msg.value, "Fallback was called");
    //}

//    function testCallFoo(address payable _addr) public payable {
//
//        (bool success, bytes memory data) = _addr.call{value: msg.value}(
//            abi.encodeWithSignature("foo(string,uint256)", "call foo", 123)
//        );
//
//        emit Response(success, data);
//    }


    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {
        emit DebugMessage(index, "Chained call of contract three!");


        // Contract two will call three twice
        if (destinations.length > index) {
            ContractOne one = ContractOne(destinations[index]);

            emit DebugMessage(index, "Chained call of contract three - one constructed!");
            emit DebugMessage(index, "Chained call of contract three - one calling...");
            //one.chainedCall(destinations, index + 1);
            (bool success) = one.chainedCall(destinations, index + 1);
            emit DebugMessage(index, "Chained call of contract three - one called...");
            //emit Response(success, data);
        } {
            emit DebugMessage(index, "Reached end of array.");
        }

        emit DebugMessage(index, "Chained call of contract three fin!");

        //(bool success, bytes memory data) = destinations[index].call(
        //    abi.encodeWithSignature("chainedCall(string,uint256)", "call chainedCall", 123)
        //);
    }

    // Calling a function that does not exist triggers the fallback function.
    function testCallDoesNotExist(address payable _addr) public payable {
        (bool success, bytes memory data) = _addr.call{value: msg.value}(
            abi.encodeWithSignature("doesNotExist()")
        );

        //emit Response(success, data);
    }
}
