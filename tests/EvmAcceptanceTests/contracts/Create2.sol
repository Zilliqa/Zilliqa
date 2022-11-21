// SPDX-License-Identifier: MIT
//pragma solidity ^0.8.14;
pragma solidity >=0.7.0 <0.9.0;

//Use Create2 to know contract address before it is deployed.
contract DeployWithCreate2 {
    address public owner;
    event Constructed(address indexed addr, string message);

    constructor(address _owner) {
        owner = _owner;
        emit Constructed(owner, "we managed it...");
    }

    function getTestme() public view returns (uint) {
        return 4;
    }
}

contract Create2Factory {
    event Deploy(address indexed addr, address indexed second);
    event DeployExtra(address indexed addr, address indexed second, address indexed third);
    event PrintMe(string message);
    DeployWithCreate2 public child;

    //function childAddress() public view returns (address payable) {
    //  return payable(address(child));
    //}

    function getTestmeTwo() public view returns (address) {
        emit DeployExtra(address(child), address(child), address(child));
        return address(child);
    }

    // to deploy another contract using owner address and salt specified
    function deploy(uint _salt) external {
        emit Deploy(address(child), address(child));
        emit PrintMe("we are here");
        emit PrintMe("we are here1");

        child = new DeployWithCreate2{
            salt: bytes32(_salt)    // the number of salt determines the address of the contract that will be deployed
        }(msg.sender);

        emit PrintMe("we are here2");

        emit Deploy(address(child), address(child));
    }

    // get the computed address before the contract DeployWithCreate2 deployed using Bytecode of contract DeployWithCreate2 and salt specified by the sender
    function getAddress(bytes memory bytecode, uint _salt) public view returns (address) {
        bytes32 hash = keccak256(
            abi.encodePacked(
                bytes1(0xff), address(this), _salt, keccak256(bytecode)
            )
        );
        return address (uint160(uint(hash)));
    }
    // get the ByteCode of the contract DeployWithCreate2
    function getBytecode(address _owner) public pure returns (bytes memory) {
        bytes memory bytecode = type(DeployWithCreate2).creationCode;
        return abi.encodePacked(bytecode, abi.encode(_owner));
    }
}

