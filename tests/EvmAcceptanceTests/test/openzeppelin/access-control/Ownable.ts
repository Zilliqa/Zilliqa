import {expect} from "chai";
import {ethers} from "hardhat";

describe("Openzeppelin ownable contract functionality", function () {
  before(async function () {
    const Contract = await ethers.getContractFactory("OwnableBox");
    this.contract = await Contract.deploy();
    await this.contract.deployed();
  });

  it("should return the deployer as the owner", async function () {
    const [owner] = await ethers.getSigners();
    expect(await this.contract.owner()).to.be.equal(owner.address);
  });

  it("should be possible to call a restricted function using the owner account", async function () {
    expect(await this.contract.store(123))
      .to.emit(this.contract, "ValueChanged")
      .withArgs(123);
  });

  it("should not be possible to call a restricted function using an arbitrary account", async function () {
    const [_, notOwner] = await ethers.getSigners();

    await expect(this.contract.connect(notOwner).store(123)).to.be.revertedWith("Ownable: caller is not the owner");
  });

  it("should be possible to call a unrestricted function", async function () {
    const [_, notOwner] = await ethers.getSigners();
    expect(await this.contract.connect(notOwner).retrieve()).to.be.equal(123);
  });

  it("should be possible to transfer ownership", async function () {
    const [prevOwner, newOwner] = await ethers.getSigners();

    await expect(this.contract.transferOwnership(newOwner.address))
      .to.emit(this.contract, "OwnershipTransferred")
      .withArgs(prevOwner.address, newOwner.address);
  });

  it("should not be possible to call restricted functions even by owner if renounceOwnership is called", async function () {
    // We changed the owner in previous test.
    const [_, owner] = await ethers.getSigners();

    // Sanity check
    expect(await this.contract.owner()).to.be.eq(owner.address);

    await this.contract.connect(owner).renounceOwnership();
    await expect(this.contract.connect(owner).store(123)).to.be.revertedWith("Ownable: caller is not the owner");
  });
});
