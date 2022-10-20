// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

import "@openzeppelin/contracts/token/ERC20/ERC20.sol";

contract OpenZeppelinGLDToken is ERC20 {
  constructor(uint256 initialSupply) ERC20("Gold", "GLD") {
    _mint(msg.sender, initialSupply);
  }
}