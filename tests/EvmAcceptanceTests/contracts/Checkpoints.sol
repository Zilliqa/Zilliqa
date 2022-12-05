// SPDX-License-Identifier: MIT
pragma solidity ^0.8.9;
import "@openzeppelin/contracts/utils/Checkpoints.sol";

contract CheckpointsMock {
    using Checkpoints for Checkpoints.History;

    Checkpoints.History private _totalCheckpoints;

    function push(uint256 value) public returns (uint256, uint256) {
        return _totalCheckpoints.push(value);
    }
}
