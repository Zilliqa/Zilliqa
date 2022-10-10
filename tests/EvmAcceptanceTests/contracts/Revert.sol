// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

import "hardhat/console.sol";

contract Revert {
  address owner;

  constructor() {
    console.log("Constructing RevertContract ofrom sender:", msg.sender);
    owner = msg.sender; // address that deploys contract will be the owner
  }

  function revertCall() public view {
    console.log("RevertCall from sender:", msg.sender, " to owner:", owner);
    revert();
  }

  function value() public pure returns (int256) {
    return 0xFFFF;
  }
}
