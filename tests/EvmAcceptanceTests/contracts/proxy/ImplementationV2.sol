// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity 0.8.9;

import "./ProxyStorageV2.sol";
import "./interfaces/InterfaceV2.sol";
import "./ImplementationV1.sol";

contract ImplementationV2 is ImplementationV1, ProxyStorageV2, InterfaceV2
{
    function initialize(uint256 _value) public virtual override initializer {
        _counter = 0;
        __Ownable_init();
    }

    function incrementCounter() external override virtual {
        _counter += 1;
    }

    function getCounter() public view returns (uint256) {
        return _counter;
    }

    function setValueInMap(address key, uint value) public {
        _map[key] = value;
    }

    function getValueFromMap(address key) public view returns (uint) {
        return _map[key];
    }

    function _authorizeUpgrade(address newImplementation) internal override onlyOwner {}
}