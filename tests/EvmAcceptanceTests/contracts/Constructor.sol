// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract WithStringConstructor {
    string public name;

    constructor(string memory _name) {
        name = _name;
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