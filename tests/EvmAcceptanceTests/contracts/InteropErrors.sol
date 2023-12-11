// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

pragma abicoder v2;

// A separate contract to avoid us depending too much on BasicInterop.sol (which may change .. )
contract InteropErrors {
  event EthEvent(bool);
  
  function callString(address contract_address, string memory tran_name, uint256 keep_origin, string memory value) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin,  value);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        emit EthEvent(true);
    }
}
