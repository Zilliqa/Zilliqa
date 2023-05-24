// SPDX-License-Identifier: MIT
pragma solidity ^0.8.10;

// Open Zeppelin libraries for controlling upgradability and access.
import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";
import "@openzeppelin/contracts-upgradeable/proxy/utils/UUPSUpgradeable.sol";
import "@openzeppelin/contracts-upgradeable/access/OwnableUpgradeable.sol";

// Sample upgradeable contract from https://blog.logrocket.com/using-uups-proxy-pattern-upgrade-smart-contracts/
contract PizzaPlusUUPS is Initializable, UUPSUpgradeable, OwnableUpgradeable {
   uint256 public slices;
   mapping(address => address) public addresses;

   event AddressIs(address);
   event AddressIsNot();

   ///@dev no constructor in upgradable contracts. Instead we have initializers
   ///@param _sliceCount initial number of slices for the pizza
   function initialize(uint256 _sliceCount) public initializer {
       slices = _sliceCount;

      ///@dev as there is no constructor, we need to initialise the OwnableUpgradeable explicitly
       __Ownable_init();
   }

   ///@dev required by the OZ UUPS module
   function _authorizeUpgrade(address) internal override onlyOwner {}

   ///@dev decrements the slices when called
   function eatSlice() external {
       require(slices > 1, "no slices left");
       slices -= 1;
   }

   function setAddress() external {
     addresses[msg.sender] = msg.sender;
   }

   function getAddress() external returns (address) {
     if (addresses[msg.sender] != address(0)) {
         emit AddressIs(addresses[msg.sender]);
         return addresses[msg.sender];
     } else {
       emit AddressIsNot();
       return address(0);
     }
   }

   function getAddressPure() external view returns (address) {
     return addresses[msg.sender];
   }
}
