// SPDX-License-Identifier: GPL-3.0-or-later

pragma solidity >=0.8.0 <0.9.0;

interface ERC20Interface {
  function totalSupply() external view returns (uint256);

  function balanceOf(address account) external view returns (uint256);

  function transfer(address to, uint256 amount) external returns (bool);

  function transferFrom(address src, address dst, uint128 amount) external returns (bool success);

  function approve(address spender, uint256 amount) external returns (bool);

  function allowance(address owner, address spender) external view returns (uint256);

  event Transfer(address indexed from, address indexed to, uint256 value);
  event Approval(address indexed owner, address indexed spender, uint256 value);

  event Transfer();
  event DecreasedAllowance();
  event IncreasedAllowance();
}

library ScillaConnector {
  uint private constant CALL_SCILLA_WITH_THE_SAME_SENDER = 1;
  uint private constant SCILLA_CALL_PRECOMPILE_ADDRESS = 0x5a494c53;
  uint private constant SCILLA_STATE_READ_PRECOMPILE_ADDRESS = 0x5a494c92;

  /**
   * @dev Calls a ZRC2 contract function with two arguments
   * @param target The address of the ZRC2 contract
   * @param tran_name The name of the function to call
   * @param arg1 The first argument to the function
   * @param arg2 The second argument to the function
   */
  function call(address target, string memory tran_name, address arg1, uint256 arg2) internal {
    bytes memory encodedArgs = abi.encode(target, tran_name, CALL_SCILLA_WITH_THE_SAME_SENDER, arg1, arg2);
    uint256 argsLength = encodedArgs.length;

    assembly {
      let alwaysSuccessForThisPrecompile := call(
        21000,
        SCILLA_CALL_PRECOMPILE_ADDRESS,
        0,
        add(encodedArgs, 0x20),
        argsLength,
        0x20,
        0
      )
    }
  }

  /**
   * @dev Calls a ZRC2 contract function with three arguments
   * @param target The address of the ZRC2 contract
   * @param tran_name The name of the function to call on the ZRC2 contract
   * @param arg1 The first argument to the function
   * @param arg2 The second argument to the function
   * @param arg3 The third argument to the function
   */
  function call(address target, string memory tran_name, address arg1, address arg2, uint256 arg3) internal {
    bytes memory encodedArgs = abi.encode(target, tran_name, CALL_SCILLA_WITH_THE_SAME_SENDER, arg1, arg2, arg3);
    uint256 argsLength = encodedArgs.length;

    assembly {
      let alwaysSuccessForThisPrecompile := call(
        21000,
        SCILLA_CALL_PRECOMPILE_ADDRESS,
        0,
        add(encodedArgs, 0x20),
        argsLength,
        0x20,
        0
      )
    }
  }

  /**
   * @dev Reads a 128 bit integer from a ZRC2 contract
   * @param target The address of the ZRC2 contract
   * @param variable_name The name of the variable to read from the ZRC2 contract
   * @return The value of the variable
   */
  function read_uint128(address target, string memory variable_name) internal view returns (uint128) {
    bytes memory encodedArgs = abi.encode(target, variable_name);
    uint256 argsLength = encodedArgs.length;
    bytes memory output = new bytes(36);

    assembly {
      let alwaysSuccessForThisPrecompile := staticcall(
        21000,
        SCILLA_STATE_READ_PRECOMPILE_ADDRESS,
        add(encodedArgs, 0x20),
        argsLength,
        add(output, 0x20),
        32
      )
    }

    return abi.decode(output, (uint128));
  }

  /**
   * @dev Reads a 32 bit integer from a ZRC2 contract
   * @param target The address of the ZRC2 contract
   * @param variable_name The name of the variable to read from the ZRC2 contract
   * @return The value of the variable
   */
  function read_uint32(address target, string memory variable_name) internal view returns (uint32) {
    bytes memory encodedArgs = abi.encode(target, variable_name);
    uint256 argsLength = encodedArgs.length;
    bytes memory output = new bytes(36);

    assembly {
      let alwaysSuccessForThisPrecompile := staticcall(
        21000,
        SCILLA_STATE_READ_PRECOMPILE_ADDRESS,
        add(encodedArgs, 0x20),
        argsLength,
        add(output, 0x20),
        32
      )
    }

    return abi.decode(output, (uint32));
  }

  /**
   * @dev Reads a string from a ZRC2 contract
   * @param target The address of the ZRC2 contract
   * @param variable_name The name of the variable to read from the ZRC2 contract
   * @return retVal The value of the variable
   */
  function read_string(address target, string memory variable_name) internal view returns (string memory retVal) {
    bytes memory encodedArgs = abi.encode(target, variable_name);
    uint256 argsLength = encodedArgs.length;
    bool success;
    bytes memory output = new bytes(128);
    uint256 output_len = output.length - 4;
    assembly {
      success := staticcall(
        21000,
        SCILLA_STATE_READ_PRECOMPILE_ADDRESS,
        add(encodedArgs, 0x20),
        argsLength,
        add(output, 0x20),
        output_len
      )
    }
    require(success);

    (retVal) = abi.decode(output, (string));
    return retVal;
  }

  /**
   * @dev Reads a 128 bit integer from a map in a ZRC2 contract
   * @param variable_name The name of the map in the ZRC2 contract
   * @param addressMapKey The key to the map
   * @return The value associated with the key in the map
   */
  function read_map_uint128(
    address target,
    string memory variable_name,
    address addressMapKey
  ) internal view returns (uint128) {
    bytes memory encodedArgs = abi.encode(target, variable_name, addressMapKey);
    uint256 argsLength = encodedArgs.length;
    bytes memory output = new bytes(36);

    assembly {
      let alwaysSuccessForThisPrecompile := staticcall(
        21000,
        SCILLA_STATE_READ_PRECOMPILE_ADDRESS,
        add(encodedArgs, 0x20),
        argsLength,
        add(output, 0x20),
        32
      )
    }

    return abi.decode(output, (uint128));
  }

  /**
   * @dev Reads a 128 bit integer from a nested map in a ZRC2 contract
   * @param target The address of the ZRC2 contract
   * @param variable_name The name of the map in the ZRC2 contract
   * @param firstMapKey The first key to the map
   * @param secondMapKey The second key to the map
   * @return The value associated with the keys in the map
   */
  function read_nested_map_uint128(
    address target,
    string memory variable_name,
    address firstMapKey,
    address secondMapKey
  ) internal view returns (uint128) {
    bytes memory encodedArgs = abi.encode(target, variable_name, firstMapKey, secondMapKey);
    uint256 argsLength = encodedArgs.length;
    bytes memory output = new bytes(36);

    assembly {
      let alwaysSuccessForThisPrecompile := staticcall(
        21000,
        SCILLA_STATE_READ_PRECOMPILE_ADDRESS,
        add(encodedArgs, 0x20),
        argsLength,
        add(output, 0x20),
        32
      )
    }

    return abi.decode(output, (uint128));
  }
}

contract ERC20isZRC2 is ERC20Interface {
  using ScillaConnector for address;

  address private _contract_owner;

  string public symbol;
  string public name;
  uint8 public decimals;
  address zrc2_address;

  uint256 private constant CALL_MODE = 1;
  uint256 private constant _NONEVM_CALL_PRECOMPILE_ADDRESS = 0x5a494c53;
  uint256 private constant _NONEVM_STATE_READ_PRECOMPILE_ADDRESS = 0x5a494c92;
  uint128 private constant _UINT8_MAX = 2 ** 8 - 1;

  constructor(address _zrc2_address) {
    _contract_owner = msg.sender;
    zrc2_address = _zrc2_address;

    symbol = zrc2_address.read_string("symbol");
    decimals = safeTrimFromUint32toUint8(zrc2_address.read_uint32("decimals"));
    name = zrc2_address.read_string("name");
  }

  function totalSupply() external view returns (uint256) {
    return zrc2_address.read_uint128("total_supply");
  }

  function initSupply() external view returns (uint128) {
    return zrc2_address.read_uint128("init_supply");
  }

  function balanceOf(address tokenOwner) external view returns (uint256) {
    return zrc2_address.read_map_uint128("balances", tokenOwner);
  }

  function transfer(address to, uint256 tokens) external returns (bool) {
    zrc2_address.call("Transfer", to, tokens);
    emit Transfer();
    return true;
  }

  function transferFailed(address to, uint128 tokens) external returns (bool) {
    zrc2_address.call("TransferFailed", to, tokens);
    return true;
  }

  function transferFrom(address from, address to, uint128 tokens) external returns (bool) {
    zrc2_address.call("TransferFrom", from, to, tokens);
    return true;
  }

  function allowance(address tokenOwner, address spender) external view returns (uint256) {
    return zrc2_address.read_nested_map_uint128("allowances", tokenOwner, spender);
  }

  function approve(address spender, uint256 new_allowance) external returns (bool) {
    uint128 current_allowance = zrc2_address.read_nested_map_uint128("allowances", msg.sender, spender);
    if (current_allowance >= new_allowance) {
      zrc2_address.call("DecreaseAllowance", spender, current_allowance - new_allowance);
      emit DecreasedAllowance();
    } else {
      zrc2_address.call("IncreaseAllowance", spender, new_allowance - current_allowance);
      emit IncreasedAllowance();
    }
    return true;
  }

  function mint(address to, uint128 tokens) external returns (bool) {
    require(msg.sender == _contract_owner, "Only allowed for contract owner");
    zrc2_address.call("Mint", to, tokens);
    return true;
  }

  function burn(address to, uint128 tokens) external returns (bool) {
    require(msg.sender == _contract_owner, "Only allowed for contract owner");
    zrc2_address.call("Burn", to, tokens);
    return true;
  }

  /**
   * @dev Asserts that a value is a valid 128 bit integer
   * @param value The value to be checked
   * @return The original value cast to a uint128
   */
  function safeTrimFromUint32toUint8(uint32 value) internal pure returns (uint8) {
    require(value <= _UINT8_MAX, "value greater than uint8 max value");
    return uint8(value);
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

contract ContractSupportingScillaReceiver is ERC165 {
  function supportsInterface(bytes4 interfaceID) external view returns (bool) {
    return
      interfaceID == this.supportsInterface.selector || // ERC165
      interfaceID == this.handle_scilla_message.selector; // ScillaReceiver
  }

  function handle_scilla_message(string memory, bytes calldata) external payable {}
}
