// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity 0.8.9;

import "./ProxyStorageV1.sol";
import "./interfaces/InterfaceV1.sol";
import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";
import "@openzeppelin/contracts-upgradeable/proxy/utils/UUPSUpgradeable.sol";
import "@openzeppelin/contracts-upgradeable/access/OwnableUpgradeable.sol";

contract ImplementationV1 is ProxyStorage, Initializable, OwnableUpgradeable, UUPSUpgradeable, InterfaceV1
{
    function initialize(uint256 _value) public virtual initializer {
        _secretValue = _value;
        __Ownable_init();
    }

    function setSecretValue(uint256 _value) external override virtual {
        _secretValue = _value;
    }

    function getSecretValue() public view returns (uint256) {
        return _secretValue;
    }

    function _authorizeUpgrade(address) internal override virtual onlyOwner {}
}