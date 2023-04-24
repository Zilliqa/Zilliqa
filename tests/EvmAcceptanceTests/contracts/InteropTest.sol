// SPDX-License-Identifier: GPL-3.0-or-later

pragma solidity >=0.7.0 <0.9.0;

contract InteropTestSol {
  address payable scilla_address;

  constructor(address payable _scilla_address) {
    scilla_address = _scilla_address;
  }

  function receiveFunds() external payable {
    // Nothing.
  }

  function Ick() external payable {
    _call_scilla_two_args("Ock", address(this), 0);
  }

  function sendMoneyToScilla(uint128 amount) external payable {
    scilla_address.transfer(amount);
  }

  function sendCallToScilla(uint128 ignoreme) external payable {
     _call_scilla_two_args("sayHelloAndReceiveFunds", address(this), 0);
  }

  function sendCallToScilla2(uint128 ignoreme) external payable {
     _call_scilla_two_args_origin("sayHelloAndReceiveFunds", address(this), 0);
  }
  
  function tick() external payable {
    _call_scilla_two_args("Tock", address(this), 0);
  }

  function _call_scilla_two_args(string memory tran_name, address recipient, uint128 amount) private {
        bytes memory encodedArgs = abi.encode(scilla_address, tran_name, recipient, amount);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c51, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
    }

  // Keeps the origin of the call so you can pass calls along.
  function _call_scilla_two_args_origin(string memory tran_name, address recipient, uint128 amount) private {
        bytes memory encodedArgs = abi.encode(scilla_address, tran_name, recipient, amount);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c52, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);
  }
}
