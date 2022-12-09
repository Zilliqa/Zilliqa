const {expect} = require("chai");
const {ethers} = require("hardhat");
const ethers_helper = require("../helper/EthersHelper");

const FUND = ethers.utils.parseUnits("2", "ether");

describe("ForwardZil contract functionality", function () {
  let contract;

  before(async function () {
    this.contract = await ethers_helper.deployContract("ForwardZil");
    this.signer = this.contract.signer;
  });

  it("Should return zero as the initial balance of the contract", async function () {
    expect(await ethers.provider.getBalance(this.contract.address)).to.be.eq(0);
  });

  it(`Should move ${ethers.utils.formatEther(FUND)} ethers to the contract if deposit is called`, async function () {
    expect(await this.contract.deposit({value: FUND})).changeEtherBalance(this.contract.address, FUND);
  });

  // TODO: Add notPayable contract function test.

  it("Should move 1 ether to the owner if withdraw function is called so 1 ether is left for the contract itself [@transactional]", async function () {
    expect(await this.contract.withdraw()).to.changeEtherBalances(
      [this.contract.address, this.signer.address],
      [ethers.utils.parseEther("-1.0"), ethers.utils.parseEther("1.0")],
      {includeFee: true}
    );
  });

  it("should be possible to transfer ethers to the contract", async function () {
    expect(
      await this.signer.sendTransaction({
        to: this.contract.address,
        value: FUND
      })
    ).to.changeEtherBalance(this.contract.address, FUND);
  });
});

describe("Transfer ethers", function () {
  it("should be possible to transfer ethers to a user account", async function () {
    const payee = ethers.Wallet.createRandom();

    const signer = await ethers_helper.signer();
    expect(
      await signer.sendTransaction({
        to: payee.address,
        value: FUND
      })
    ).to.changeEtherBalance(payee.address, FUND);
  });
});
