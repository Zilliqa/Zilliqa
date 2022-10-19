const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Openzeppelin ownable contract functionality", function () {
  let contract;
  before(async function () {
    const Contract = await ethers.getContractFactory("OwnableBox");
    contract = await Contract.deploy();
    await contract.deployed();
  });

  it("should return the deployer as the owner", async function () {
    const [owner] = await ethers.getSigners();
    expect(await contract.owner()).to.be.equal(owner.address);
  });

  it("should be possible to call a restricted function using the owner account", async function () {
    expect(await contract.store(123))
      .to.emit(contract, "ValueChanged")
      .withArgs(123);
  });

  // FIXME: In ZIL-4899
  xit("should not be possible to call a restricted function using an arbitrary account", async function () {
    const [_, notOwner] = await ethers.getSigners();

    await expect(contract.connect(notOwner).store(123)).to.be.revertedWith("Ownable: caller is not the owner");
  });

  it("should be possible to call a unrestricted function", async function () {
    const [_, notOwner] = await ethers.getSigners();
    expect(await contract.connect(notOwner).retrieve()).to.be.equal(123);
  });

  it("should be possible to transfer ownership", async function () {
    const [prevOwner, newOwner] = await ethers.getSigners();

    await expect(contract.transferOwnership(newOwner.address))
      .to.emit(contract, "OwnershipTransferred")
      .withArgs(prevOwner.address, newOwner.address);
  });

  // FIXME: In ZIL-4899
  xit("should not be possible to call restricted functions even by owner if renounceOwnership is called", async function () {
    // We changed the owner in previous test.
    const [_, owner] = await ethers.getSigners();

    // Sanity check
    expect(await contract.owner()).to.be.eq(owner.address);

    await contract.connect(owner).renounceOwnership();
    await expect(contract.connect(owner).store(123)).to.be.revertedWith("Ownable: caller is not the owner");
  });
});
