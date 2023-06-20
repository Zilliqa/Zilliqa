pragma solidity ^0.8.0;

// Similar to non-working proxy -> proxy code provided by @NFTs2Me in discord.
// - rrw 2023-05-06

contract DDContractA {
    mapping(uint256 => address) private _owners;

    event MsgA(string message, address owner);
    event ValueA(address val);
    event Bool(bool val);
    event Int(uint256 val);

    function setOwner(uint256 tokenId) external {
        emit MsgA("setOwnerA", msg.sender);
        _owners[tokenId] = msg.sender;
    }

    function getOwner(uint256 tokenId) public view virtual returns (address) {
        return _owners[tokenId];
    }

    function owner(uint256 tokenId) public returns (address) {
        emit ValueA(_owners[tokenId]);
        return _owners[tokenId];
    }

    /*    function _ownerOf(uint256 tokenId) internal view virtual returns (address) {
        return _owners[tokenId];
    }

    function ownerOf(uint256 tokenId) public view virtual returns (address) {
        return address(this);
    }

    function getOwnerX(uint256 val) public returns (address) {
        emit Msg("AOwnerX");
        emit Value(_owners[val]);
        return _owners[val];
    }
    */
}


// Proxy to Contract A
contract DDContractB {
    mapping(uint256 => address) private _owners;
    address public _impl;

    event MsgB(string message, address sender);
    event Value(address val);
    event Bool(bool val);
    event Int(uint256 val);

    function setImplementation(address _impl_in) external payable {
        _impl = _impl_in;
    }

    fallback() external payable virtual {
        emit MsgB("contractB", msg.sender);
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
}

// Contract C
contract DDContractC {
    address public _contract_b;

    event MsgCx(string message, address sender);
    event MsgC(string message);
    event ValueC(address val);
    event Bool(bool val);
    event Int(uint256 val);

    // msg.sender here should be ContractC (or ContractD if proxied)
    function setOwner(uint256 tokenId) public {
        emit MsgCx("SetOwnerC", msg.sender);
        try DDContractA(_contract_b).setOwner(tokenId) {
            emit MsgCx("SetOwnerC exec done", msg.sender);
            //emit Value(ownerOf);
        } catch {
            emit MsgCx("SetOwnerC except", msg.sender);
            //emit Msg("Caught exception");
        }
        emit MsgCx("SetOwnerC done", msg.sender);
        //emit Value(_contract_b);
    }

    function getOwner(uint256 tokenId) public view returns (address collectionOwner) {
        //emit Msg("Hello");
        try DDContractA(_contract_b).getOwner(tokenId) returns (address ownerOf) {
            //emit ValueC(ownerOf);
            return ownerOf;
        } catch {
            //emit Msg("Caught exception");
        }
    }

    function owner(uint256 tokenId) public returns (address collectionOwner) {
        emit MsgC("Hello");
        try DDContractA(_contract_b).owner(tokenId) returns (address ownerOf) {
            emit ValueC(ownerOf);
            return ownerOf;
        } catch {
            emit MsgC("Caught exception");
        }
    }
}

// Contract D
contract DDContractD {
    address public _contract_b;
    address public _impl;

    event MsgD(string message, address);
    event Value(address val);
    event Bool(bool val);
    event Int(uint256 val);

    function setImplementation(address _impl_in, address contract_b) external payable {
        _impl = _impl_in;
        _contract_b = contract_b;
    }

    fallback() external payable virtual {
        emit MsgD("Contract D", msg.sender);
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
}