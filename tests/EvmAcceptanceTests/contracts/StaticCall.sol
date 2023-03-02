// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract Called {

  uint public number;
  event IncrementMessage(uint, string);

  constructor() {
      number = 0;
  }

  function increment() public returns(bool, uint) {
    number++;
    emit IncrementMessage(number, "Incrementing...");
    return (true, number);
  }

  function getNumber() public view returns (uint) {
      return number;
  }

}

contract Caller {

  function callCalled(address called) public returns(bool, bytes memory) {

    (bool success, bytes memory data) = called.staticcall(abi.encodeWithSignature("increment()"));
    assert(success);

    return (success, data);
  }
}
