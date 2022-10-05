// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

contract Precompiles {

  constructor() {}

   function testRecovery(bytes32 documentHash, uint8 v, bytes32 r, bytes32 s) pure public returns (address) {
        bytes memory prefix = "\x19Ethereum Signed Message:\n32";
        bytes32 prefixedProof = keccak256(abi.encodePacked(prefix, documentHash));
        address recovered = ecrecover(prefixedProof, v, r, s);
        return recovered;
    }
}
