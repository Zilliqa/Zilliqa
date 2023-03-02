// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract Called {

  uint public number;

  function increment() public {
    number++;
    return number
  }

}

contract Caller {

  function callCalled(address called) public returns(bool, bytes memory) {

    (bool success, bytes memory data) = called.call(abi.encodeWithSignature("increment()"));

    return (success, data);
  }
}
