import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";

describe("Delegatecall functionality", function () {
  before(async function () {
    // Final level
    this.testDelegateContract = await parallelizer.deployContract("TestDelegatecall");

    // Second level
    this.delegateContract = await parallelizer.deployContract("Delegatecall");

    // First level
    this.baseDelegator = await parallelizer.deployContract("BaseDelegator");
  });

  //it("should delegate function call correctly", async function () {
  //  const VALUE = 1000000;
  //  const NUM = 3735931646; // 0xDEADCAFE

  //  const owner = this.delegateContract.signer;
  //  await this.delegateContract.setVars(this.testDelegateContract.address, NUM, {value: VALUE});

  //  expect(await this.delegateContract.num()).to.be.eq(NUM);
  //  expect(await this.delegateContract.value()).to.be.eq(VALUE);
  //  expect(await this.delegateContract.sender()).to.be.eq(owner.address);
  //  expect(await ethers.provider.getBalance(this.delegateContract.address)).to.be.eq(VALUE);

  //  expect(await this.testDelegateContract.num()).to.be.eq(0);
  //  expect(await this.testDelegateContract.value()).to.be.eq(0);
  //  expect(await this.testDelegateContract.sender()).to.be.eq("0x0000000000000000000000000000000000000000");
  //  expect(await ethers.provider.getBalance(this.testDelegateContract.address)).to.be.eq(0);
  //});

  it("Can see the values of delegated calls chained multiple times", async function () {
    const VALUE = 1000000;

    //await this.baseDelegator.setOwnerAndCheck(this.delegateContract.address, this.testDelegateContract.address, {value: VALUE});

    // Set up the ID mapping
    await this.testDelegateContract.setOwnerAndCheck();

    await this.baseDelegator.checkOwnerTwoWays(this.testDelegateContract.address, this.delegateContract.address);
  });
});
