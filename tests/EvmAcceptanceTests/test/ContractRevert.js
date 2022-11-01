const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Revert Contract Call", function () {
  let contract;
  before(async function () {
    const Contract = await ethers.getContractFactory("Revert");
    contract = await Contract.deploy();
  });

  it("Will revert the contract when revert is called", async function () {
    await expect(contract.revertCall()).to.be.reverted;
  });

  it("Should return revert error message if the called function reverts with custom message", async function () {
    const REVERT_MESSAGE = "reverted!!";
    await expect(contract.revertCallWithMessage(REVERT_MESSAGE)).to.be.revertedWith(REVERT_MESSAGE);
  });

  it("Should return revert error object if the called function reverts with custom error", async function () {
    const [owner] = await ethers.getSigners();
    await expect(contract.revertCallWithCustomError({value: 1000}))
      .to.be.revertedWithCustomError(contract, "FakeError")
      .withArgs(1000, owner.address);
  });
});
