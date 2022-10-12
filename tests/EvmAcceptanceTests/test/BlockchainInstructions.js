const {expect} = require("chai");
const {ethers} = require("hardhat");

// TODO: Change the description to something more meaningful.
describe("Blockchain Instructions contract", function () {
  let contract;
  before(async function () {
    const Contract = await ethers.getContractFactory("BlockchainInstructions");
    contract = await Contract.deploy();
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should return the owner address when getOrigin function is called", async function () {
    const [owner] = await ethers.getSigners();
    expect(await contract.getOrigin()).to.be.eq(owner.address);
  });
});
