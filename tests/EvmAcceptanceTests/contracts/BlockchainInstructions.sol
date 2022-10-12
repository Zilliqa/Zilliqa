// SPDX-License-Identifier: GPL-3.0-or-later

pragma solidity ^0.8.7;

contract BlockchainInstructions {
    address public _origin;
    address public _coinbase;
    uint256 public _gasprice;
    bytes32 public _blockHash;
    uint256 public _timestamp;
    uint256 public _difficulty;
    uint256 public _gaslimit;

    function origin() external returns (address) {
        _origin = tx.origin;
        return _origin;
    }

    function getOrigin() external view returns (address) {
        return tx.origin;
    }

    function coinbase() external returns (address) {
        _coinbase = block.coinbase;
        return _coinbase;
    }

    function getCoinbase() external view returns (address) {
        return block.coinbase;
    }

    function gasprice() external returns (uint256) {
        _gasprice = tx.gasprice;
        return _gasprice;
    }

    // There is not transaction in the view, so let's see how that works.
    function getGasprice() external view returns (uint256) {
        return tx.gasprice;
    }

    function blockHash(uint blockNumber) external returns (bytes32) {
        _blockHash = blockhash(blockNumber);
        return _blockHash;
    }

    function getBlockHash(uint blockNumber) external view returns (bytes32) {
        return blockhash(blockNumber);
    }

    function timestamp() external returns (uint256) {
        _timestamp = block.timestamp;
        return _timestamp;
    }

    function getTimestamp() external view returns (uint256) {
        return block.timestamp;
    }

    function difficulty() external returns (uint256) {
        _difficulty = block.difficulty;
        return _difficulty;
    }

    function getDifficulty() external view returns (uint256) {
        return block.difficulty;
    }
    function gaslimit() external returns (uint256) {
        _gaslimit = block.gaslimit;
        return _gaslimit;
    }

    function getGaslimit() external view returns (uint256) {
        return block.gaslimit;
    }
    
    // TODO implement basefee similarly.
}
