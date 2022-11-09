const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Delegatecall functionality", function () {
  before(async function () {
    const Contract = await ethers.getContractFactory("Delegatecall");
    this.delegateContract = await Contract.deploy();

    const TestContract = await ethers.getContractFactory("TestDelegatecall");
    this.testDelegateContract = await TestContract.deploy();
  });

  it("should delegate function call correctly", async function () {
    const VALUE = 1000;
    const NUM = 123;

    const [owner] = await ethers.getSigners();
    await this.delegateContract.setVars(this.testDelegateContract.address, NUM, {value: VALUE});
    expect(await this.delegateContract.num()).to.be.eq(NUM);
    expect(await this.delegateContract.value()).to.be.eq(VALUE);
    expect(await this.delegateContract.sender()).to.be.eq(owner.address);
    expect(await ethers.provider.getBalance(this.delegateContract.address)).to.be.eq(VALUE);

    expect(await this.testDelegateContract.num()).to.be.eq(0);
    expect(await this.testDelegateContract.value()).to.be.eq(0);
    expect(await this.testDelegateContract.sender()).to.be.eq("0x0000000000000000000000000000000000000000");
    expect(await ethers.provider.getBalance(this.testDelegateContract.address)).to.be.eq(0);
  });
});
