// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract ZeroParamConstructor {
    uint256 public number;

    constructor() {
        number = 123;
    }

    function getNumber() private view returns (uint256) {
        return number;
    }
}

contract WithStringConstructor {
    string public name;

    constructor(string memory _name) {
        name = _name;
    }
}

contract WithUintConstructor {
    uint public number;

    constructor(uint _number) {
        number = _number;
    }
}

contract WithAddressConstructor {
    address public someAddress;

    constructor(address _addr) {
        someAddress = _addr;
    }
}

contract WithEnumConstructor {
    enum MyEnum { Zero, One, Two }

    MyEnum public someEnum;

    constructor(MyEnum _enum) {
        someEnum = _enum;
    }
}


contract MultiParamConstructor {
    string public name;
    uint public number;

    constructor(string memory _name, uint _number) {
        name = _name;
        number = _number;
    }
}

contract WithPayableConstructor {
    address public owner;
    uint public balance;

    constructor() payable{
        balance = msg.value;
        owner = msg.sender;
    }
}