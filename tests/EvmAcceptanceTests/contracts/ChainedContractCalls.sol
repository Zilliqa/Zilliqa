// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

// Entry contract, will call contract two
contract ContractOne {

    event DebugMessage(uint index, string message);

    constructor() payable
    {
        emit DebugMessage(0, "Contract one constructor...");
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {
        emit DebugMessage(index, "Chained call of contract one!");

        if (destinations.length > index) {
            ContractTwo two = ContractTwo(destinations[index]);
            (bool success) = two.chainedCall(destinations, index + 1);
        }

        emit DebugMessage(index, "Chained call of contract one fin!");
    }
}

// Second contract, will call contract three, twice
contract ContractTwo {

    event DebugMessage(uint index, string message);

    constructor() payable
    {
        emit DebugMessage(0, "Contract two constructor...");
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {
        emit DebugMessage(index, "Chained call of contract two!");

        // Contract two will call three TWICE
        if (destinations.length > index) {
            ContractThree three = ContractThree(destinations[index]);
            three.chainedCall(destinations, index + 1);
            (bool success) = three.chainedCall(destinations, index + 1);
        }

        emit DebugMessage(index, "Chained call of contract two fin!");
    }
}

// Last contract, will call contract one
contract ContractThree {

    event DebugMessage(uint index, string message);

    constructor() payable
    {
        emit DebugMessage(0, "Contract three constructor...");
    }

    function chainedCall(address payable[] memory destinations, uint index) public returns(bool success)
    {
        emit DebugMessage(index, "Chained call of contract three!");

        // Contract two will call three twice
        if (destinations.length > index) {
            ContractOne one = ContractOne(destinations[index]);

            (bool success) = one.chainedCall(destinations, index + 1);
        }
        emit DebugMessage(index, "Chained call of contract three fin!");
    }

    // Calling a function that does not exist triggers the fallback function.
    function testCallDoesNotExist(address payable _addr) public payable {
        (bool success, bytes memory data) = _addr.call{value: msg.value}(
            abi.encodeWithSignature("doesNotExist()")
        );
    }
}
