const {expect} = require("chai");
const {ethers} = require("hardhat");

const FUND = ethers.utils.parseUnits("2", "ether");

describe("ForwardZil contract functionality", function () {
  let contract;

  before(async function () {
    const Contract = await ethers.getContractFactory("ForwardZil");
    contract = await Contract.deploy();
  });

  it("Should return zero as the initial balance of the contract", async function () {
    expect(await ethers.provider.getBalance(contract.address)).to.be.eq(0);
  });

  it(`Should move ${ethers.utils.formatEther(FUND)} ethers to the contract if deposit is called`, async function () {
    expect(await contract.deposit({value: FUND})).changeEtherBalance(contract.address, FUND);
  });

  // TODO: Add notPayable contract function test.

  it("Should move 1 ether to the owner if withdraw function is called so 1 ether is left for the contract itself [@transactional]", async function () {
    const [owner] = await ethers.getSigners();
    expect(await contract.withdraw()).to.changeEtherBalances(
      [contract.address, owner.address],
      [ethers.utils.parseEther("-1.0"), ethers.utils.parseEther("1.0")],
      {includeFee: true}
    );
  });

  it("should be possible to transfer ethers to the contract", async function () {
    const [payer] = await ethers.getSigners();
    expect(
      await payer.sendTransaction({
        to: contract.address,
        value: FUND
      })
    ).to.changeEtherBalance(contract.address, FUND);
  });
});

describe("Transfer ethers", function () {
  it("should be possible to transfer ethers to a user account", async function () {
    const payee = ethers.Wallet.createRandom();
    const [payer] = await ethers.getSigners();

    expect(
      await payer.sendTransaction({
        to: payee.address,
        value: FUND
      })
    ).to.changeEtherBalance(payee.address, FUND);
  });
});
