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
 
 
interface ZRC2ERC20Interface {
    function totalSupplyERC20() external view returns (uint128);
    function totalSupplyZRC2() external view returns (uint128);
    function balanceOfERC20(address tokenOwner) external view returns (uint128);
    function balanceOfZRC2(address tokenOwner) external view returns (uint128);
    function allowanceERC20(address tokenOwner, address spender) external view returns (uint128);
    function allowanceZRC2(address tokenOwner, address spender) external view returns (uint128);
    function approveERC20(address tokenOwner, uint128 tokens) external returns (bool);
    function approveZRC2(address tokenOwner, uint128 tokens) external returns (bool);
    function transferERC20(address to, uint128 tokens) external returns (bool);
    function transferZRC2(address to, uint128 tokens) external returns (bool);
    function transferFromERC20(address from, address to, uint128 tokens) external returns (bool);
    function transferFromZRC2(address from, address to, uint128 tokens) external returns (bool);

    function mintZRC2(address recipient, uint128 amount) external returns (bool);
    function burnZRC2(address recipient, uint128 amount) external returns (bool);

    // The ones below allow to transfer between zrc2 and erc20
    function transferErc20ToZrc2(address owner, uint128 tokens) external returns (bool);
    function transferZrc2ToErc20(address owner, uint128 tokens) external returns (bool);
}

contract ERC20Interop is ZRC2ERC20Interface, SafeMath {
    mapping(address => uint128) balances;
    mapping(address => mapping(address => uint128)) allowed;
    uint128 _totalSupply;

    address private _contract_owner;
    address private _zrc2_address;

    uint256 private constant CALL_MODE = 1;
 
    constructor(address zrc2_address) {
        _contract_owner = msg.sender;
        _zrc2_address = zrc2_address;
    }
 
    function totalSupplyERC20() external view returns (uint128) {
        return _totalSupply;
    }

    function totalSupplyZRC2() external view returns (uint128) {
        return _read_scilla_uint128("total_supply");
    }

 
    function balanceOfERC20(address tokenOwner) external view returns (uint128) {
        return balances[tokenOwner];
    }

    function balanceOfZRC2(address tokenOwner) external view returns (uint128) {
        return _read_scilla_map_uint128("balances", tokenOwner);
    }
 
    function transferERC20(address to, uint128 tokens) external returns (bool) {
        balances[msg.sender] = safeSub(balances[msg.sender], tokens);
        balances[to] = safeAdd(balances[to], tokens);
        return true;
    }

    function transferZRC2(address to, uint128 tokens) external returns (bool) {
        _call_scilla_two_args("Transfer", to, tokens);
        return true;
    }
 
    function transferFromERC20(address from, address to, uint128 tokens) external returns (bool) {
        balances[from] = safeSub(balances[from], tokens);
        allowed[from][msg.sender] = safeSub(allowed[from][msg.sender], tokens);
        balances[to] = safeAdd(balances[to], tokens);
        return true;
    }

    function transferFromZRC2(address from, address to, uint128 tokens) external returns (bool) {
        _call_scilla_three_args("TransferFrom", from, to, tokens);
        return true;
    }
 
    function allowanceERC20(address tokenOwner, address spender) external view returns (uint128) {
        return allowed[tokenOwner][spender];
    }

    function allowanceZRC2(address tokenOwner, address spender) external view returns (uint128) {
        return _read_scilla_nested_map_uint128("allowances", tokenOwner, spender);
    }

    function approveERC20(address spender, uint128 tokens) external returns (bool) {
        allowed[msg.sender][spender] = tokens;
        return true;
    }

    function approveZRC2(address spender, uint128 new_allowance) external returns (bool) {
        uint128 current_allowance = _read_scilla_nested_map_uint128("allowances", msg.sender, spender);
        if (current_allowance >= new_allowance) {
            _call_scilla_two_args("DecreaseAllowance", spender, new_allowance);
        }
        else {
            _call_scilla_two_args("IncreaseAllowance", spender, new_allowance);
        }
        return true;
    }

    function mintZRC2(address to, uint128 tokens) external returns (bool) {
        require(msg.sender == _contract_owner, "Only allowed for contract owner");
        _call_scilla_two_args("Mint", to, tokens);
        return true;
    }
    
    function burnZRC2(address to, uint128 tokens) external returns (bool) {
        require(msg.sender == _contract_owner, "Only allowed for contract owner");
        _call_scilla_two_args("Burn", to, tokens);
        return true;
    }

    function transferErc20ToZrc2(address to, uint128 tokens) external returns (bool) {
        require(msg.sender == _contract_owner, "Only allowed for contract owner");
        balances[to] = safeSub(balances[to], tokens);
        _totalSupply = safeSub(_totalSupply, tokens);
        _call_scilla_two_args("Mint", to, tokens);
        return true;
    }

    function transferZrc2ToErc20(address to, uint128 tokens) external returns (bool) {
        require(msg.sender == _contract_owner, "Only allowed for contract owner");
        _call_scilla_two_args("Burn", to, tokens);
         balances[to] = safeAdd(balances[to], tokens);
        _totalSupply = safeAdd(_totalSupply, tokens);
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
 
}
