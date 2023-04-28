// SPDX-License-Identifier: SEE LICENSE IN LICENSE
pragma solidity ^0.8.0;

//pragma solidity ^0.8.19;
//import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";

import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";
import "@openzeppelin/contracts-upgradeable/proxy/utils/UUPSUpgradeable.sol";
import "@openzeppelin/contracts-upgradeable/access/OwnableUpgradeable.sol";

//import "@openzeppelin/contracts-upgradeable/proxy/utils/UUPSUpgradeable.sol";
//

// This contract is not to be called directly. This is the proxy implementation of Contract B
contract A {
    mapping(uint256 => address) _owners;

    ///// @custom:oz-upgrades-unsafe-allow constructor
    //constructor() {
    //    _disableInitializers();
    //}

    //function initialize() public payable initializer {
    //}

    function _ownerOf(uint256 tokenId) internal view virtual returns (address) {
        return _owners[tokenId];
    }

    function ownerOf(uint256 tokenId) public view virtual returns (address) {
        address owner = _ownerOf(tokenId);
        require(owner != address(0), "ERC721: invalid token ID");
        return owner;
    }
}

// UUPS proxy of contract A
contract Nathan is A, Initializable, UUPSUpgradeable, OwnableUpgradeable {

   //uint256 public slices;
   //mapping(uint256 => address) private _owners;

   //@dev no constructor in upgradable contracts. Instead we have initializers
   ///@param owner initial number xxx
   function initialize(uint256 owner) public initializer {
       _owners[owner] = msg.sender;

      ///@dev as there is no constructor, we need to initialise the OwnableUpgradeable explicitly
       __Ownable_init();
   }

   ///@dev required by the OZ UUPS module
   function _authorizeUpgrade(address) internal override onlyOwner {}

   /////@dev decrements the slices when called
   //function eatSlice() external {
   //    require(slices > 1, "no slices left");
   //    slices -= 1;
   //}
}

// Contract B is just a UUPS proxy of Contract A (https://github.com/OpenZeppelin/openzeppelin-contracts/blob/v4.8.3/contracts/proxy/utils/UUPSUpgradeable.sol).
// Most probably any proxy would work, even a minimal proxy.
// [4:15 PM]
// So you’d need to first deploy the implementation, and over that, deploy a proxy of Contract A (Contract B) that calls the initialize() method (actually not needed for the example).
// Then, the storage is actually held by Contract B, and Contract A only holds the implementation, but no data.
// [4:15 PM]
// Then, we have contract C (implementation) and D (minimal proxy over contract C).
// [4:17 PM]
// I have to leave, so I’ll be quick on this
// [4:19 PM]
// For Contract C.
// 11:02

//function owner() public view returns (address collectionOwner) {
//    try CONTRACT_A(proxyAddressContractB).ownerOf(uint256(uint160(address(this)))) returns (address ownerOf) {
//        return ownerOf;
//    } catch {}
//}
