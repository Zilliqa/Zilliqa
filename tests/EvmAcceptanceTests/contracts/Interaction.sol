// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract WithSettersAndGetters {
    uint256 public number;
    string public name;

    enum MyEnum { Zero, One, Two }

    MyEnum public someEnum;
    address public someAddress;

    event logWithoutParam();
    event logWithUint256Param(uint256 value);
    event logWithStringPram(string param); 

    /// Public Setters
    function setNumber(uint256 _number) public {
        number = _number;
    }

    function setName(string memory _name) public {
        name = _name;
    }

    /// External Setters
    function setNumberExternal(uint256 _number) external {
        number = _number;
    }

    function setNameExternal(string memory _name) external {
        name = _name;
    }

    function setEnum(MyEnum _enum) public {
        someEnum = _enum;
    }

    function setAddress(address _address) public {
        someAddress = _address;
    }

    /// Public Views

    function getEnum() external view returns(MyEnum) {
        return someEnum;
    }

    function getNumberPublic() external view returns(uint256) {
        return number;
    }

    function getNamePublic() external view returns(string memory) {
        return name;
    }

    // External Views
    function getNumberExternal() external view returns(uint256) {
        return number;
    }

    function getNameExternal() external view returns(string memory) {
        return name;
    }

    // Public Pure Functions
    function getOne() public pure returns(uint256) {
        return 1;
    }

    // External Pure Functions
    function getTwo() external pure returns(uint256) {
        return 2;
    }

    // Log generators
    function emitLogWithoutParam() public {
        emit logWithoutParam();
    }

    function emitLogWithUint256Param() public {
        emit logWithUint256Param(234);
    }
}