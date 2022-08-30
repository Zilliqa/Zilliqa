// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract WithSettersAndGetters {
    uint256 public number;
    string public name;

    function setNumber(uint256 _number) public {
        number = _number;
    }

    function setName(string memory _name) public {
        name = _name;
    }

    function setNumberExternal(uint256 _number) external {
        number = _number;
    }

    function setNameExternal(string memory _name) external {
        name = _name;
    }

    function getNumberExternal() external view returns(uint256) {
        return number;
    }

    function getNameExternal() external view returns(string memory) {
        return name;
    }

    function getOne() public pure returns(uint256) {
        return 1;
    }

    function getTwo() external pure returns(uint256) {
        return 2;
    }
}