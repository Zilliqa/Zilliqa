import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";

describe("Delegatecall functionality", function () {
  before(async function () {
    this.delegateContract = await parallelizer.deployContract("Delegatecall");
    this.testDelegateContract = await parallelizer.deployContract("TestDelegatecall");
  });

  it("should stop delegating at a call boundary", async function() {
    const VALUE = 1000000;
    // gas estimator can't estimate, so ..
    const GAS_LIMIT = 500000;
    const DEADCAFE = 3735931646; // 0xDEADCAFE
    const DEADBEEF = 3735928559; // 0xDEADBEEF
    const F00DF00D = 4027445261; 
    const owner = this.delegateContract.signer;

    await this.delegateContract.setVarsCall(this.testDelegateContract.address, DEADCAFE, {gasLimit: GAS_LIMIT, value: VALUE});

    expect(await this.delegateContract.num()).to.be.eq(DEADBEEF); 
    expect(await this.delegateContract.value()).to.be.eq(F00DF00D);
    expect(await this.delegateContract.sender()).to.be.eq("0x0000000000000000000000000000000000000000");
    expect(await ethers.provider.getBalance(this.delegateContract.address)).to.be.eq(VALUE);

    expect(await this.testDelegateContract.num()).to.be.eq(42);
    expect(await this.testDelegateContract.value()).to.be.eq(0);
    expect(await this.testDelegateContract.sender()).to.be.eq(this.delegateContract.address);
    expect(await ethers.provider.getBalance(this.testDelegateContract.address)).to.be.eq(0);

    // Now return the funds.
    await this.delegateContract.unwind(this.delegateContract.signer.address,
                                       this.testDelegateContract.address);
    expect(await ethers.provider.getBalance(this.delegateContract.address)).to.be.eq(0);
  });

  it("should delegate function call correctly", async function () {
    const VALUE = 1000000;
    const NUM = 3735931646; // 0xDEADCAFE

    const owner = this.delegateContract.signer;
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
