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

  function testEcAdd(uint256 a_x, uint256 a_y, uint256 b_x, uint256 b_y) public returns (uint256[2] memory p) {
    uint256[4] memory input;
    input[0] = a_x;
    input[1] = a_y;
    input[2] = b_x;
    input[3] = b_y;
    assembly {
      // input size  = (256 / 8) * 4 = 0x80
      // output size = (256 / 8) * 2 = 0x40
      if iszero(call(gas(), 0x06, 0, input, 0x80, p, 0x40)) {
        revert(0, 0)
      }
    }
  }

  function testEcMul(uint256 p_x, uint256 p_y, uint256 s) public returns (uint256[2] memory p) {
    uint256[3] memory input;
    input[0] = p_x;
    input[1] = p_y;
    input[2] = s;
    assembly {
      // input size  = (256 / 8) * 3 = 0x60
      // output size = (256 / 8) * 2 = 0x40
      if iszero(call(gas(), 0x07, 0, input, 0x60, p, 0x40)) {
        revert(0, 0)
      }
    }
  }

  function testEcPairing(uint256[] memory pairs) public returns (uint256) {
    uint256 elements = pairs.length;
    assembly {
      // `pairs` is prefixed by its length, so we jump forward one integer to skip this prefix
      // output size = (256 / 8) * 1 = 0x20
      if iszero(call(gas(), 0x08, 0, add(pairs, 0x20), mul(elements, 0x20), add(pairs, 0x20), 0x20)) {
        revert(0, 0)
      }
    }
    return pairs[0];
  }

  function testBlake2(uint32 rounds, bytes32[2] memory h, bytes32[4] memory m, bytes8[2] memory t, bool f) public returns (bytes32[2] memory) {
    bytes32[2] memory output;

    bytes memory args = abi.encodePacked(rounds, h[0], h[1], m[0], m[1], m[2], m[3], t[0], t[1], f);

    assembly {
      if iszero(call(gas(), 0x09, 0, add(args, 32), 0xd5, output, 0x40)) {
        revert(0, 0)
      }
    }

    return output;
  }
}
