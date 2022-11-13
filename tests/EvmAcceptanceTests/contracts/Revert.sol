// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

import "hardhat/console.sol";

error FakeError(uint256 value, address sender);

contract Revert {
  address owner;
  uint256[] private _array;

  constructor() {
    console.log("Constructing RevertContract from sender:", msg.sender);
    owner = msg.sender; // address that deploys contract will be the owner
  }

  function revertCall() public payable {
    revert();
  }

  function revertCallWithMessage(string memory revertMessage) public payable {
    revert(revertMessage);
  }

  function revertCallWithCustomError() public payable {
    revert FakeError({value: msg.value, sender: msg.sender});
  }

  function value() public pure returns (int256) {
    return 0xFFFF;
  }

  function outOfGas() public {
    for (uint256 i = 0; ; ++i) {
        _array.push(i);
    }
  }
}
