import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";

const FUND = ethers.utils.parseUnits("1", "gwei");

describe("ForwardZil contract functionality", function () {
  before(async function () {
    this.contract = await parallelizer.deployContract("ForwardZil");
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
    const prevBalance = await ethers.provider.getBalance(this.contract.address);
    await parallelizer.sendTransaction({
      to: this.contract.address,
      value: FUND
    });

    const currentBalance = await ethers.provider.getBalance(this.contract.address);
    expect(currentBalance.sub(prevBalance)).to.be.eq(FUND);
  });
});

describe("Transfer ethers", function () {
  it("should be possible to transfer ethers to a user account", async function () {
    const payee = ethers.Wallet.createRandom();

    await parallelizer.sendTransaction({
      to: payee.address,
      value: FUND
    });

    expect(await ethers.provider.getBalance(payee.address)).to.be.eq(FUND);
  });
});

it("should be possible to batch transfer using a smart contract", async function () {
  const ACCOUNTS_COUNT = 3;
  const ACCOUNT_VALUE = 1_000_000;

  const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
    ethers.Wallet.createRandom().connect(ethers.provider)
  );

  const addresses = accounts.map((signer) => signer.address);

  const BatchTransferContract = await ethers.getContractFactory("BatchTransfer");
  const batchTransfer = await BatchTransferContract.deploy(addresses, ACCOUNT_VALUE, {
    value: ACCOUNTS_COUNT * ACCOUNT_VALUE
  });
  await batchTransfer.deployed();

  const balances = await Promise.all(accounts.map((account) => account.getBalance()));
  balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));
});
