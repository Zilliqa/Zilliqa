const {expect} = require("chai");
const {ethers} = require("hardhat");

const FUND = ethers.utils.parseUnits("2", "ether");

describe("Transfer functionality", function () {
  let contract;

  before(async function () {
    const Contract = await ethers.getContractFactory("ForwardZil");
    contract = await Contract.deploy();
  });

  it("Should return zero as the initial balance of the contract", async function () {
    expect(await ethers.provider.getBalance(contract.address)).to.be.eq(0);
  });

  it(`Should move ${ethers.utils.formatEther(FUND)} ethers to the contract if deposit is called`, async function () {
    await contract.deposit({value: FUND});
    expect(await ethers.provider.getBalance(contract.address)).to.be.eq(FUND);
  });

  // TODO: Add notPayable contract function test.

  // FIXME: In ZIL-4890
  xit("Should move 1 ether to the owner if withdraw function is called so 1 ether is left for the contract itself [@transactional]", async function () {
    const [owner] = await ethers.getSigners();
    await expect(contract.withdraw())
      .to.changeEtherBalance(contract.address, ethers.utils.parseEther("-1.0"))
      .to.changeEtherBalance(owner.address, ethers.utils.parseEther("1.0"));
  });
});
