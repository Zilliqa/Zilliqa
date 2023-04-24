// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

contract GasTransferTest {
  address payable public owner;
  uint256 public count;
  
  constructor() payable {
    owner = payable(msg.sender);
  }
  function takeAllMyMoney() public payable {
    count += 1;
  }
  
  function sendback(uint amount) public {
    owner.transfer(amount);
    count += 2;
  }

  function blowUp() public {
    selfdestruct(owner);
  }
   
}
