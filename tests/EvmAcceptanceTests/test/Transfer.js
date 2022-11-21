const {expect} = require("chai");
const {ethers} = require("hardhat");

const FUND = ethers.utils.parseUnits("2", "ether");

describe("ForwardZil contract functionality", function () {
  before(async function () {
    const Contract = await ethers.getContractFactory("ForwardZil");
    this.contract = await Contract.deploy();
  });

  it("Should return zero as the initial balance of the contract", async function () {
    expect(
      await ethers.provider.getBalance(this.contract.address),
      `Contract Address: ${this.contract.address}`
    ).to.be.eq(0);
  });

  it(`Should move ${ethers.utils.formatEther(FUND)} ethers to the contract if deposit is called`, async function () {
    expect(await this.contract.deposit({value: FUND}), `Contract Address: ${this.contract.address}`).changeEtherBalance(
      this.contract.address,
      FUND
    );
  });

  // TODO: Add notPayable contract function test.

  it("Should move 1 ether to the owner if withdraw function is called so 1 ether is left for the contract itself [@transactional]", async function () {
    const [owner] = await ethers.getSigners();
    expect(await this.contract.withdraw(), `Contract Address: ${this.contract.address}`).to.changeEtherBalances(
      [this.contract.address, owner.address],
      [ethers.utils.parseEther("-1.0"), ethers.utils.parseEther("1.0")],
      {includeFee: true}
    );
  });

  it("should be possible to transfer ethers to the contract", async function () {
    const [payer] = await ethers.getSigners();
    expect(
      await payer.sendTransaction({
        to: this.contract.address,
        value: FUND
      }),
      `Contract Address: ${this.contract.address}`
    ).to.changeEtherBalance(this.contract.address, FUND);
  });
});

describe("Transfer ethers", function () {
  it("should be possible to transfer ethers to a user account", async function () {
    const payee = ethers.Wallet.createRandom();
    const [payer] = await ethers.getSigners();

    const txn = await payer.sendTransaction({
      to: payee.address,
      value: FUND
    });

    expect(await ethers.provider.getBalance(payee.address), `Txn Hash: ${txn.hash}`).to.be.eq(FUND);
  });
});
