// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

// Similar to non-working proxy -> proxy code provided by @NFTs2Me in discord.
// - rrw 2023-05-06

contract DDContractA {
  mapping(uint256 => address) private _owners;

  event Msg(string message);
  event Value(address val);
  event Bool(bool val);
  event Int(uint256 val);

  function setOwner(uint256 tokenId) external {
    emit Msg("SetOwnerA");
    _owners[tokenId] = msg.sender;
  }

  function _ownerOf(uint256 tokenId) internal view virtual returns (address) {
    return _owners[tokenId];
  }

  function ownerOf(uint256 tokenId) public view virtual returns (address) {
    address owner = _ownerOf(tokenId);
    require(owner != address(0), "ERC721: Invalid token ID");
    return owner;
  }

  function getOwnerX(uint256 val) public returns (address) {
    emit Msg("AOwnerX");
    emit Value(_owners[val]);
    return _owners[val];
  }

}


// Proxy to Contract A
contract DDContractB {
  mapping(uint256 => address) private _owners;
  address public _impl;

  event Msg(string message);
  event Value(address val);
  event Bool(bool val);
  event Int(uint256 val);

  function setImplementation(address _impl_in) external payable {
    _impl = _impl_in;
  }

  function getOwnerX(uint256 val) public returns (address) {
    emit Msg("BOwnerX");
    emit Value(_owners[val]);
    return _owners[val];
  }

  fallback() external payable virtual {
    _delegate(_impl);
  }

  function _delegate(address implementation) internal virtual {
    assembly {
      // Copy msg.data. We take full control of memory in this inline assembly
      // block because it will not return to Solidity code. We overwrite the
      // Solidity scratch pad at memory position 0.
      calldatacopy(0, 0, calldatasize())
          // Call the implementation.
          // out and outsize are 0 because we don't know the size yet.
          let result := delegatecall(gas(), implementation, 0, calldatasize(), 0, 0)

          // Copy the returned data.
          returndatacopy(0, 0, returndatasize())

            switch result
            // delegatecall returns 0 on error.
            case 0 {
                revert(0, returndatasize())
            }
            default {
                return(0, returndatasize())
            }
        }
    }

  // Exists purely to get the solidity compiler to shut up about the lack of one
  receive() external payable {
    // Do nothing.
  }
}

// Contract C
contract DDContractC {
  address public _contract_b;

  event Msg(string message);
  event Value(address val);
  event Bool(bool val);
  event Int(uint256 val);


  // msg.sender here should be ContractC (or ContractD if proxied)
  function setOwner(uint256 tokenId) public {
    emit Msg("SetOwnerC");
    DDContractA(_contract_b).setOwner(tokenId);
    emit Msg("SetOwnerC done");
    emit Value(_contract_b);
  }

  function owner() public returns (address collectionOwner) {
    emit Msg("Hello");
    try DDContractA(_contract_b).ownerOf(uint256(uint160(address(this)))) returns (address ownerOf) {
      emit Value(ownerOf);
      return ownerOf;
    } catch {
      emit Msg("Caught exception");
    }
  }
}

// Contract D
contract DDContractD {
  address public _contract_b;
  address public _impl;

  event Msg(string message);
  event Value(address val);
  event Bool(bool val);
  event Int(uint256 val);

  function setImplementation(address _impl_in, address _contract_b_in) external payable {
    _impl = _impl_in;
    _contract_b = _contract_b_in;
  }

  fallback() external payable virtual {
    _delegate(_impl);
  }

  function _delegate(address implementation) internal virtual {
    assembly {
      // Copy msg.data. We take full control of memory in this inline assembly
      // block because it will not return to Solidity code. We overwrite the
      // Solidity scratch pad at memory position 0.
      calldatacopy(0, 0, calldatasize())
          // Call the implementation.
          // out and outsize are 0 because we don't know the size yet.
          let result := delegatecall(gas(), implementation, 0, calldatasize(), 0, 0)

          // Copy the returned data.
          returndatacopy(0, 0, returndatasize())

            switch result
            // delegatecall returns 0 on error.
            case 0 {
                revert(0, returndatasize())
            }
            default {
                return(0, returndatasize())
            }
        }
    }

  // Exists purely to get the solidity compiler to shut up about the lack of one
  receive() external payable {
    // Do nothing.
  }
}
