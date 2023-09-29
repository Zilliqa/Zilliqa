import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";

describe("Openzeppelin ownable contract functionality #parallel", function () {
  let contract: Contract;
  let newOwner: SignerWithAddress;
  before(async function () {
    contract = await hre.deployContract("OwnableBox");
  });

  it("should return the deployer as the owner @block-1", async function () {
    const owner = contract.signer as SignerWithAddress;
    expect(await contract.owner()).to.be.equal(owner.address);
  });

  it("should be possible to call a restricted function using the owner account @block-1", async function () {
    expect(await contract.store(123))
      .to.emit(contract, "ValueChanged")
      .withArgs(123);
  });

  it("should not be possible to call a restricted function using an arbitrary account @block-1", async function () {
    const notOwner = hre.allocateEthSigner();

    await expect(contract.connect(notOwner).store(123)).to.be.revertedWith("Ownable: caller is not the owner");

    hre.releaseEthSigner(notOwner);
  });

  it("should be possible to call a unrestricted function @block-2", async function () {
    const notOwner = hre.allocateEthSigner();
    expect(await contract.connect(notOwner).retrieve()).to.be.equal(123);
    hre.releaseEthSigner(notOwner);
  });

  it("should be possible to transfer ownership @block-2", async function () {
    const prevOwner = contract.signer as SignerWithAddress;
    newOwner = hre.allocateEthSigner();

    await expect(contract.transferOwnership(newOwner.address))
      .to.emit(contract, "OwnershipTransferred")
      .withArgs(prevOwner.address, newOwner.address);
  });

  it("should not be possible to call restricted functions even by owner if renounceOwnership is called", async function () {
    // We changed the owner in previous test.
    await contract.connect(newOwner).renounceOwnership();
    await expect(contract.connect(newOwner).store(123)).to.be.revertedWith("Ownable: caller is not the owner");
    hre.releaseEthSigner(newOwner);
  });
});
