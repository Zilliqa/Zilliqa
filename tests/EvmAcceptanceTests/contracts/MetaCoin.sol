// SPDX-License-Identifier: MIT
// Tells the Solidity compiler to compile only from v0.8.13 to v0.9.0
pragma solidity >=0.7.0 <0.9.0;

// This is just a simple example of a coin-like contract.
// It is not ERC20 compatible and cannot be expected to talk to other
// coin/token contracts.

contract MetaCoin {
	mapping (address => uint) balances;

	event Transfer(address indexed _from, address indexed _to, uint256 _value);

	constructor() {
		balances[tx.origin] = 10000;
	}

	function sendCoin(address receiver, uint amount) public returns(bool sufficient) {
		if (balances[msg.sender] < amount) return false;
		balances[msg.sender] -= amount;
		balances[receiver] += amount;
		emit Transfer(msg.sender, receiver, amount);
		return true;
	}

	// function getBalanceInEth(address addr) public view returns(uint){
	// 	return ConvertLib.convert(getBalance(addr),2);
	// }

	function getBalance(address addr) public view returns(uint) {
		return balances[addr];
	}
}
