// SPDX-License-Identifier: GPL-3.0-or-later

pragma solidity >=0.7.0 <0.9.0;

// Safe Math Interface.
contract SafeMath {

    function safeAdd(uint128 a, uint128 b) public pure returns (uint128 c) {
        c = a + b;
        require(c >= a);
    }

    function safeSub(uint128 a, uint128 b) public pure returns (uint128 c) {
        require(b <= a);
        c = a - b;
    }

    function safeMul(uint128 a, uint128 b) public pure returns (uint128 c) {
        c = a * b;
        require(a == 0 || c / a == b);
    }

    function safeDiv(uint128 a, uint128 b) public pure returns (uint128 c) {
        require(b > 0);
        c = a / b;
    }
}


interface ERC20Interface {
    function totalSupply() external view returns (uint128);
    function balanceOf(address tokenOwner) external view returns (uint128);
    function allowance(address tokenOwner, address spender) external view returns (uint128);
    function approve(address tokenOwner, uint128 tokens) external returns (bool);
    function transfer(address to, uint128 tokens) external returns (bool);
    function transferFrom(address from, address to, uint128 tokens) external returns (bool);

    function mint(address recipient, uint128 amount) external returns (bool);
    function burn(address recipient, uint128 amount) external returns (bool);

    event TransferEvent();
    event IncreasedAllowanceEvent();
    event DecreasedAllowanceEvent();
}

contract ERC20isZRC2 is ERC20Interface, SafeMath {
    address private _contract_owner;
    address private _zrc2_address;

    uint256 private constant CALL_MODE = 1;

    constructor(address zrc2_address) {
        _contract_owner = msg.sender;
        _zrc2_address = zrc2_address;
    }

    function totalSupply() external view returns (uint128) {
        return _read_scilla_uint128("total_supply");
    }

    function initSupply() external view returns (uint128) {
        return _read_scilla_uint128("init_supply");
    }

    function tokenName() external view returns (string memory) {
        return _read_scilla_string("name");
    }

    function balanceOf(address tokenOwner) external view returns (uint128) {
        return _read_scilla_map_uint128("balances", tokenOwner);
    }

    function transfer(address to, uint128 tokens) external returns (bool) {
        _call_scilla_two_args("Transfer", to, tokens);
        emit TransferEvent();
        return true;
    }

    function transferFailed(address to, uint128 tokens) external returns (bool) {
        _call_scilla_two_args("TransferFailed", to, tokens);
        return true;
    }

    function transferFrom(address from, address to, uint128 tokens) external returns (bool) {
        _call_scilla_three_args("TransferFrom", from, to, tokens);
        return true;
    }

    function allowance(address tokenOwner, address spender) external view returns (uint128) {
        return _read_scilla_nested_map_uint128("allowances", tokenOwner, spender);
    }

    function approve(address spender, uint128 new_allowance) external returns (bool) {
        uint128 current_allowance = _read_scilla_nested_map_uint128("allowances", msg.sender, spender);
        if (current_allowance >= new_allowance) {
            _call_scilla_two_args("DecreaseAllowance", spender, current_allowance - new_allowance);
            emit DecreasedAllowanceEvent();
        }
        else {
            _call_scilla_two_args("IncreaseAllowance", spender, new_allowance - current_allowance);
            emit IncreasedAllowanceEvent();
        }
        return true;
    }

    function mint(address to, uint128 tokens) external returns (bool) {
        require(msg.sender == _contract_owner, "Only allowed for contract owner");
        _call_scilla_two_args("Mint", to, tokens);
        return true;
    }

    function burn(address to, uint128 tokens) external returns (bool) {
        require(msg.sender == _contract_owner, "Only allowed for contract owner");
        _call_scilla_two_args("Burn", to, tokens);
        return true;
    }

    // Private functions used for accessing ZRC2 contract

    function _call_scilla_two_args(string memory tran_name, address recipient, uint128 amount) private {
        bytes memory encodedArgs = abi.encode(_zrc2_address, tran_name, CALL_MODE, recipient, amount);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
    }

    function _call_scilla_three_args(string memory tran_name, address from, address to, uint128 amount) private {
        bytes memory encodedArgs = abi.encode(_zrc2_address, tran_name, CALL_MODE, from, to, amount);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
    }

    function _read_scilla_uint128(string memory variable_name) private view returns (uint128 supply) {
        bytes memory encodedArgs = abi.encode(_zrc2_address, variable_name);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(36);
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), 32)
        }
        require(success);
        (supply) = abi.decode(output, (uint128));
        return supply;
    }

    function _read_scilla_map_uint128(string memory variable_name, address owner) private view returns (uint128 allowance) {
        bytes memory encodedArgs = abi.encode(_zrc2_address, variable_name, owner);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(36);
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), 32)
        }
        require(success);
        (allowance) = abi.decode(output, (uint128));
        return allowance;
    }

    function _read_scilla_nested_map_uint128(string memory variable_name, address owner, address spender) private view returns (uint128 allowance) {
        bytes memory encodedArgs = abi.encode(_zrc2_address, variable_name, owner, spender);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(36);
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), 32)
        }
        require(success);
        (allowance) = abi.decode(output, (uint128));
        return allowance;
    }

    function _read_scilla_string(string memory variable_name) public view returns (string memory retVal) {
        bytes memory encodedArgs = abi.encode(_zrc2_address, variable_name);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(128);
        uint256 output_len = output.length - 4;
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), output_len)
        }
        require(success);

        (retVal) = abi.decode(output, (string));
        return retVal;
    }

}

interface ERC165 {
    /// @notice Query if a contract implements an interface
    /// @param interfaceID The interface identifier, as specified in ERC-165
    /// @dev Interface identification is specified in ERC-165. This function
    ///  uses less than 30,000 gas.
    /// @return `true` if the contract implements `interfaceID` and
    ///  `interfaceID` is not 0xffffffff, `false` otherwise
    function supportsInterface(bytes4 interfaceID) external view returns (bool);
}

interface ScillaReceiver {
    function handle_scilla_message(string memory, bytes calldata) external payable;
}

contract ContractSupportingScillaReceiver is ERC165{
    function supportsInterface(bytes4 interfaceID) external view returns (bool) {
        return
        interfaceID == this.supportsInterface.selector || // ERC165
        interfaceID == this.handle_scilla_message.selector; // ScillaReceiver
    }

    function handle_scilla_message(string memory, bytes calldata) external payable {}
}
