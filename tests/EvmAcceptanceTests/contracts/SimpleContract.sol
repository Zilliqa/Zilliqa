// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

contract SimpleContract {
  int256 private value = 0;

  constructor() {}

  function getInt256() public view returns (int256) {
    return value;
  }

  function setInt256(int256 _value) public {
    value = _value;
  }
}
