// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

pragma abicoder v2;

contract ScillaCall {
    function callScilla(address contract_address, string memory tran_name, int256 keep_origin, address recipient, uint128 value) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, recipient, value);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
    }

    function callForwardScilla(address contract_address, string memory tran_name, int256 keep_origin, address interScilla,
                               address evmRecipient, uint128 value) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, interScilla, evmRecipient, value);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
    }

    function callScillaInsufficientParams(address contract_address, string memory tran_name, int256 keep_origin, address recipient) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, recipient);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
    }
}
