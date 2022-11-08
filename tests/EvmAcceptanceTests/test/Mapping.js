const {expect} = require("chai");
const {ethers} = require("hardhat");

const FUND = ethers.utils.parseUnits("2", "ether");

describe("Mapping contract functionality", function () {
  let contract;

  before(async function () {
    const Contract = await ethers.getContractFactory("WithMapping");
    contract = await Contract.deploy();
  });

  it("Should return zero as the initial balance of the contract", async function () {
    const signers = await ethers.getSigners();
    await Promise.all(signers.map((payee) => contract.setAllowed(payee.address, true)));
  });
});
