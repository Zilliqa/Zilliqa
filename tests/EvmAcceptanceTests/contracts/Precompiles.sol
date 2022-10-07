// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

contract Precompiles {

  bytes public idStored;
  uint256 public modExpResult;

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
  
  function testModexp( uint _base, uint _exp, uint _modulus) public {
        modExpResult = _modExp(_base, _exp, _modulus);
  }

  function _modExp(uint256 base, uint256 exponent, uint256 modulus) internal returns (uint256 result) {
        assembly {
            let pointer := mload(0x40)
            mstore(pointer, 0x20)
            mstore(add(pointer, 0x20), 0x20)
            mstore(add(pointer, 0x40), 0x20)
            mstore(add(pointer, 0x60), base)
            mstore(add(pointer, 0x80), exponent)
            mstore(add(pointer, 0xa0), modulus)
            let value := mload(0xc0)
            if iszero(call(gas(), 0x05, 0, pointer, 0xc0, value, 0x20)) {
                revert(0, 0)
            }
            result := mload(value)
        }
  }

}
