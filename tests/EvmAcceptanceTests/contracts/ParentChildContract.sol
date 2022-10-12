// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract ChildContract {
    uint256 data;
    uint256 public value;
    address payable public sender;

    constructor(uint256 _data) payable {
       data = _data;
       value = msg.value;
       sender = payable(msg.sender);
    }

    function read() public view returns (uint256) {
       return data;
    }

    function returnToSender() public {
       uint amount = address(this).balance;
       (bool success, ) = sender.call{value: amount}("");
       require(success, "Failed to send Ether");
       selfdestruct(sender);
    }
}

contract ParentContract {
    
    ChildContract public child;
    uint256 public value;

    constructor () payable {
      value = msg.value;
    }

    function installChild(uint256 initial_data) public returns (address payable) {
      child = new ChildContract{value: value}(initial_data);
      return payable(address(child));
    }

    function childAddress() public view returns (address payable) {
      return payable(address(child));
    }

    function getPaidValue() public view returns (uint256) {
      return value;
    }

    function returnToSenderAndDestruct(address account) public payable {
      selfdestruct(payable(account));
    }

    receive() external payable {
    }
}