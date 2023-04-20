import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";

describe("Delegatecall functionality", function () {
  before(async function () {
    this.delegateContract = await parallelizer.deployContract("Delegatecall");
    this.testDelegateContract = await parallelizer.deployContract("TestDelegatecall");
  });

  it("should delegate function call correctly", async function () {
    const VALUE = 1000000;
    const NUM = 123;

    const owner = this.delegateContract.signer;

    console.log("0");
    await this.delegateContract.setVars(this.testDelegateContract.address, NUM, {value: VALUE});
    console.log("0");
    expect(await this.delegateContract.num()).to.be.eq(NUM);
    console.log("0");
    expect(await this.delegateContract.value()).to.be.eq(VALUE);
    console.log("0");
    expect(await this.delegateContract.sender()).to.be.eq(owner.address);
    console.log("0");
    expect(await ethers.provider.getBalance(this.delegateContract.address)).to.be.eq(VALUE);
    console.log("0");

    expect(await this.testDelegateContract.num()).to.be.eq(0);
    console.log("0");
    expect(await this.testDelegateContract.value()).to.be.eq(0);
    console.log("0");
    expect(await this.testDelegateContract.sender()).to.be.eq("0x0000000000000000000000000000000000000000");
    console.log("0");
    expect(await ethers.provider.getBalance(this.testDelegateContract.address)).to.be.eq(0);
  });
});
