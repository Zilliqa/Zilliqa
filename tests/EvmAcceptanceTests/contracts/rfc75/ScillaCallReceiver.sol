// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

pragma abicoder v2;

interface ERC165Interface {
    /// @notice Query if a contract implements an interface
    /// @param interfaceID The interface identifier, as specified in ERC-165
    /// @dev Interface identification is specified in ERC-165. This function
    ///  uses less than 30,000 gas.
    /// @return `true` if the contract implements `interfaceID` and
    ///  `interfaceID` is not 0xffffffff, `false` otherwise
    function supportsInterface(bytes4 interfaceID) external view returns (bool);
}

contract ScillaCallReceiver is ERC165Interface {
    function supportsInterface(bytes4 interfaceID) external view returns (bool) {
        return
        interfaceID == this.supportsInterface.selector || // ERC165
        interfaceID == this.handle_scilla_message.selector; // ScillaReceiver
    }

    function handle_scilla_message(string memory, bytes calldata) external payable {}

    function callScilla(address contract_address, string memory tran_name, int256 keep_origin, address recipient, uint128 value) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, recipient, value);
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
