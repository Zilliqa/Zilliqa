// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

pragma abicoder v2;

contract ScillaCallComplex {
    function SetAndGet(address contract_address, string memory tran_name, int256 keep_origin, address recipient, uint128 value) public {
        bytes memory encodedArgs = abi.encode(contract_address, tran_name, keep_origin, recipient, value);
        uint256 argsLength = encodedArgs.length;
        bool success;
        assembly {
            success := call(21000, 0x5a494c53, 0, add(encodedArgs, 0x20), argsLength, 0x20, 0)
        }
        require(success);

        uint128 readValue = readUint(contract_address, "value");
        if(readValue != value) {
            revert();
        }

        uint simpleMapVal = readSimpleMap(contract_address, "simpleMap", recipient);
        if(simpleMapVal != value) {
            revert();
        }

        uint nestedMapVal = readNestedMap(contract_address, "nestedMap", recipient, recipient);
        if(nestedMapVal != value) {
            revert();
        }

    }

    function readUint(address scilla_contract, string memory field_name) public view returns (uint128) {
        bytes memory encodedArgs = abi.encode(scilla_contract, field_name);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(36);
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), 32)
        }
        require(success);
        uint128 funds;

        (funds) = abi.decode(output, (uint128));
        return funds;
    }

    function readSimpleMap(address scilla_contract, string memory field_name, address idx1) public view returns (uint128 funds) {
        bytes memory encodedArgs = abi.encode(scilla_contract, field_name, idx1);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(36);
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), 32)
        }
        require(success);
        (funds) = abi.decode(output, (uint128));
        return funds;

    }

    function readNestedMap(address scilla_contract, string memory field_name, address idx1, address idx2) public view returns (uint128 funds) {
        bytes memory encodedArgs = abi.encode(scilla_contract, field_name, idx1, idx2);
        uint256 argsLength = encodedArgs.length;
        bool success;
        bytes memory output = new bytes(36);
        assembly {
            success := staticcall(21000, 0x5a494c92, add(encodedArgs, 0x20), argsLength, add(output, 0x20), 32)
        }
        require(success);
        (funds) = abi.decode(output, (uint128));
        return funds;

    }

}
