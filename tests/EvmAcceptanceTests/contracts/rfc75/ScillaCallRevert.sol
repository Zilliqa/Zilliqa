// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

pragma abicoder v2;

contract ScillaCallRevert {
    function callScilla(address contract_address, string memory tran_name, int256 keep_origin, uint128 value,
                        address recipient1, address recipient2) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, value, recipient1, recipient2);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
    }

    function callScillaChain(address contract_address, string memory tran_name, int256 keep_origin, uint128 value,
        address next_scilla_address, address recipient1, address recipient2) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, value, next_scilla_address, recipient1, recipient2);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
    }
}