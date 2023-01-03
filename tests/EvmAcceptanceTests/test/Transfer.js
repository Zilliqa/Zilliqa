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

  xit("should be possible to batch transfer using a smart contract with full precision", async function () {
    const ACCOUNTS_COUNT = 3;
    const ACCOUNT_VALUE = 1_234_567;

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

  it("probably should be possible to use sent funds of smart contract", async function () {
    const ACCOUNTS_COUNT = 1;
    const ACCOUNT_VALUE = 0;
    const TRANSFER_VALUE = 9;

    const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
      ethers.Wallet.createRandom().connect(ethers.provider)
    );

    const addresses = accounts.map((signer) => signer.address);

    let bal = await ethers.provider.getBalance(addresses[0]);
    console.log("Initial balance of account to send is: ", bal);

    const [owner] = await ethers.getSigners();
    bal = await ethers.provider.getBalance(owner.address);

    console.log("Initial balance of account sending from is: ", bal);

    // Deploy with no funds at contract address, but send value
    const SingleTransferContract = await ethers.getContractFactory("SingleTransfer");

    // Transfer the amount, WITH FUNDS
    //const singleTransfer = await SingleTransferContract.deploy({ value: 1_000_000 });
    const singleTransfer = await SingleTransferContract.deploy();
    await singleTransfer.deployed();

    console.log("arghmeemnadf ");

    const ret = await singleTransfer.doTransfer(addresses[0], TRANSFER_VALUE, {gasLimit: 25000000, value: TRANSFER_VALUE});

    console.log("arghmeemnadf ", ret);

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    //balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));

    bal = await ethers.provider.getBalance(addresses[0]);
    console.log("Final balance of account to send is: ", bal);

    bal = await ethers.provider.getBalance(owner.address);

    console.log("Final balance of account sending from is: ", bal);
  });
});
