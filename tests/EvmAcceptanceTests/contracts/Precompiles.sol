// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

contract Precompiles {

  bytes public idStored;

  constructor() {}

  function testRecovery(bytes32 documentHash, uint8 v, bytes32 r, bytes32 s) pure public returns (address) {
      bytes memory prefix = "\x19Ethereum Signed Message:\n32";
      bytes32 prefixedProof = keccak256(abi.encodePacked(prefix, documentHash));
      address recovered = ecrecover(prefixedProof, v, r, s);
      return recovered;
  }

  function testIdentity(bytes memory data) public {
      bytes memory result = new bytes(data.length);
      assembly {
        let len := mload(data)
          if iszero(call(gas(), 0x04, 0, add(data, 0x20), len, add(result,0x20), len)) {
            invalid()
          }
        }
      idStored = result;
  }

  function testSHA256(string memory word) pure public returns (bytes32) {
        bytes32 hash = sha256(bytes (word));
        return hash;        
  }

  function testRipemd160(string memory word) pure public returns (bytes20) {
        bytes20 hash = ripemd160(bytes (word));
        return hash;        
  }

}
