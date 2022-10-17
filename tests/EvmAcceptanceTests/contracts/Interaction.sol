// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract WithSettersAndGetters {
    uint256 public number;
    string public name;

    enum MyEnum { Zero, One, Two }

    MyEnum public someEnum;
    address public someAddress;

    event logWithoutParam();
    event logAnonymous() anonymous;
    event logAnonymousWithParam(string param) anonymous;
    event logWithUint256Param(uint256 value);
    event logWithStringParam(string param); 
    event logWithEnumParam(MyEnum param); 
    event logWithAddressParam(address param); 
    event logWithIndexedAddressParam(address indexed param); 
    event logWithMultiParams(uint256 param1, string param2);

    /// Public Setters
    function setNumber(uint256 _number) public {
        number = _number;
    }

    function setName(string memory _name) public {
        name = _name;
    }

    function setAddress(address _address) public {
        someAddress = _address;
    }

    function setEnum(MyEnum _enum) public {
        someEnum = _enum;
    }

    /// External Setters
    function setNumberExternal(uint256 _number) external {
        number = _number;
    }

    function setNameExternal(string memory _name) external {
        name = _name;
    }

    function setAddressExternal(address _address) external {
        someAddress = _address;
    }

    function setEnumExternal(MyEnum _enum) external {
        someEnum = _enum;
    }

    /// Public Views
    function getEnumPublic() public view returns(MyEnum) {
        return someEnum;
    }

    function getNumberPublic() public view returns(uint256) {
        return number;
    }

    function getStringPublic() public view returns(string memory) {
        return name;
    }

    function getAddressPublic() public view returns(address) {
        return someAddress;
    }

    // External Views
    function getNumberExternal() external view returns(uint256) {
        return number;
    }

    function getNameExternal() external view returns(string memory) {
        return name;
    }

    function getAddressExternal() external view returns(address) {
        return someAddress;
    }

    function getEnumExternal() external view returns(MyEnum) {
        return someEnum;
    }

    // Public Pure Functions
    function getNumberPure() public pure returns(uint256) {
        return 1;
    }

    function getStringPure() public pure returns(string memory) {
        return "Zilliqa";
    }

    function getEnumPure() public pure returns(MyEnum) {
        return MyEnum.Two;
    }

    function getTuplePure() public pure returns(uint256, string memory) {
        return (123, "zilliqa");
    }

    // External Pure Functions
    function getNumberPureExternal() external pure returns(uint256) {
        return 1;
    }

    function getStringPureExternal() external pure returns(string memory) {
        return "Zilliqa";
    }

    function getEnumPureExternal() external pure returns(MyEnum) {
        return MyEnum.Two;
    }

    function getTuplePureExternal() external pure returns(uint256, string memory) {
        return (123, "zilliqa");
    }

    // Log generators
    function emitLogWithoutParam() public {
        emit logWithoutParam();
    }

    function emitLogWithUint256Param() public {
        emit logWithUint256Param(234);
    }

    function emitLogWithStringParam() public {
        emit logWithStringParam("zilliqa");
    }

    function emitLogWithEnumParam() public {
        emit logWithEnumParam(MyEnum.Zero);
    }

    function emitLogWithAddressParam() public {
        emit logWithAddressParam(address(this));
    }

    function emitLogWithMultiParams() public {
        emit logWithMultiParams(123, "zilliqa");
    }

    function emitLogAnonymous() public {
        emit logAnonymous();
    }

    function emitLogAnonymousWithParam() public {
        emit logAnonymousWithParam("zilliqa");
    }
}