import {assert, expect} from "chai";
import {BigNumber, Contract} from "ethers";
import hre, {ethers} from "hardhat";
import {TransactionRequest} from "@ethersproject/abstract-provider";

const FUND = ethers.utils.parseUnits("1", "gwei");

async function getFee(hash: string) {
  const res = await ethers.provider.getTransactionReceipt(hash);
  return res.gasUsed.mul(res.effectiveGasPrice);
}

describe("ForwardZil contract functionality #parallel", function () {
  let contract: Contract;
  before(async function () {
    contract = await hre.deployContract("ForwardZil");
  });

  it("Should return zero as the initial balance of the contract @block-1", async function () {
    expect(await ethers.provider.getBalance(contract.address)).to.be.eq(0);
  });

  it(`Should move ${ethers.utils.formatEther(
    FUND
  )} ethers to the contract if deposit is called @block-1`, async function () {
    await contract.deposit({value: FUND});
    expect(await ethers.provider.getBalance(contract.address)).to.be.eq(FUND);
  });

  // TODO: Add notPayable contract function test.

  it("Should move 1 ether to the owner if withdraw function is called so 1 ether is left for the contract itself [@transactional]", async function () {
    expect(await contract.withdraw()).to.changeEtherBalances(
      [contract.address, await contract.signer.getAddress()],
      [ethers.utils.parseEther("-1.0"), ethers.utils.parseEther("1.0")],
      {includeFee: true}
    );
  });

  it("should be possible to transfer ethers to the contract @block-2", async function () {
    const prevBalance = await ethers.provider.getBalance(contract.address);
    const {response} = await hre.sendEthTransaction({
      to: contract.address,
      value: FUND
    });

    // Get transaction receipt for the tx
    await response.wait();

    const currentBalance = await ethers.provider.getBalance(contract.address);
    expect(currentBalance.sub(prevBalance)).to.be.eq(FUND);
  });
});

describe("Transaction types", function () {
  it("should be possible to send a legacy ethereum transaction", async function () {
    const payee = ethers.Wallet.createRandom();

    const {response} = await hre.sendEthTransaction({
      to: payee.address,
      value: FUND,
      type: 0
    });

    const receipt = await response.wait();
    expect(receipt.type).to.be.eq(0);

    expect(response.type).to.be.eq(0);
  });

  it("should be possible to send a EIP-2930 ethereum transaction", async function () {
    const payee = ethers.Wallet.createRandom();

    const accessList = [
      {
        address: ethers.Wallet.createRandom().address,
        storageKeys: ["0x0bcad17ecf260d6506c6b97768bdc2acfb6694445d27ffd3f9c1cfbee4a9bd6d"]
      }
    ];

    const {response} = await hre.sendEthTransaction({
      to: payee.address,
      value: FUND,
      type: 1,
      accessList: accessList
    });

    const receipt = await response.wait();
    expect(receipt.type).to.be.eq(1);

    expect(response.type).to.be.eq(1);
    expect(response.accessList).to.have.lengthOf(1);
  });

  it("should be possible to send a EIP-1557 ethereum transaction", async function () {
    const payee = ethers.Wallet.createRandom();

    const accessList = [
      {
        address: ethers.Wallet.createRandom().address,
        storageKeys: ["0x0bcad17ecf260d6506c6b97768bdc2acfb6694445d27ffd3f9c1cfbee4a9bd6d"]
      }
    ];

    const {response} = await hre.sendEthTransaction({
      to: payee.address,
      value: FUND,
      type: 2,
      accessList: accessList,
      maxFeePerGas: 100
    });

    const receipt = await response.wait();
    expect(receipt.type).to.be.eq(2);

    expect(response.type).to.be.eq(2);
    expect(response.accessList).to.have.lengthOf(1);
    expect(response.maxPriorityFeePerGas).to.be.not.null;
    expect(response.maxFeePerGas).to.be.not.null;
  });
});

describe("Transfer ethers #parallel", function () {
  it("should be possible to transfer ethers to a user account @block-1", async function () {
    const payee = ethers.Wallet.createRandom();

    const {response} = await hre.sendEthTransaction({
      to: payee.address,
      value: FUND
    });

    await response.wait();

    expect(await ethers.provider.getBalance(payee.address)).to.be.eq(FUND);
  });

  it("should be possible to batch transfer using a smart contract @block-1", async function () {
    const ACCOUNTS_COUNT = 3;
    const ACCOUNT_VALUE = 1_000_000;

    const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
      ethers.Wallet.createRandom().connect(ethers.provider)
    );

    const addresses = accounts.map((signer) => signer.address);

    await hre.deployContract("BatchTransferCtor", addresses, ACCOUNT_VALUE, {
      value: ACCOUNTS_COUNT * ACCOUNT_VALUE
    });

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));
  });

  it("should be possible to batch transfer using a smart contract and get funds back on self destruct @block-1", async function () {
    const ACCOUNTS_COUNT = 3;
    const ACCOUNT_VALUE = ethers.utils.parseEther("0.1");

    const [owner] = await ethers.getSigners();
    let initialOwnerBal = await ethers.provider.getBalance(owner.address);

    const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
      ethers.Wallet.createRandom().connect(ethers.provider)
    );

    const addresses = accounts.map((signer) => signer.address);

    const batchTrans = await hre.deployContract("BatchTransferCtor", addresses, ACCOUNT_VALUE, {
      value: ACCOUNT_VALUE.mul(ACCOUNTS_COUNT + 2)
    });

    const fee1 = await getFee(batchTrans.deployTransaction.hash);

    // Make sure to remove gas accounting from the calculation
    let finalOwnerBal = await ethers.provider.getBalance(owner.address);
    const diff = initialOwnerBal.sub(finalOwnerBal).sub(fee1);

    // We will see that our account is down 5x, selfdestruct should have returned the untransfered funds
    if (diff > ACCOUNT_VALUE.mul(4)) {
      assert.equal(true, false, "We did not get a full refund from the selfdestruct. Balance drained: " + diff);
    }

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));
  });

  it("probably should be possible to use sent funds of smart contract", async function () {
    const TRANSFER_VALUE = 1_000_000;

    // Create random account
    const rndAccount = ethers.Wallet.createRandom().connect(ethers.provider);
    const randomAccount = rndAccount.address;

    let initialBal = await ethers.provider.getBalance(randomAccount);
    expect(initialBal).to.be.eq(0);

    const owner = hre.allocateEthSigner();
    let InitialOwnerbal = await ethers.provider.getBalance(owner.address);

    // check enough funds + gas
    expect(InitialOwnerbal).to.be.at.least(TRANSFER_VALUE * 1.1);

    // Deploy the contract
    const singleTransfer = await hre.deployContractWithSigner("SingleTransfer", owner);

    // call SC with a value to move funds across
    await singleTransfer.doTransfer(randomAccount, TRANSFER_VALUE, {
      gasLimit: 25000000,
      value: TRANSFER_VALUE
    });

    const receivedBal = await ethers.provider.getBalance(randomAccount);

    expect(receivedBal).to.be.eq(TRANSFER_VALUE);
    hre.releaseEthSigner(owner);
  });

  it("probably should be possible to simulate transfer via eth_call", async function () {
    const TRANSFER_VALUE = 1_000_000;

    // Create random account
    const rndAccount = ethers.Wallet.createRandom().connect(ethers.provider);
    const randomAccount = rndAccount.address;

    let initialBal = await ethers.provider.getBalance(randomAccount);
    expect(initialBal).to.be.eq(0);

    const owner = hre.allocateEthSigner();
    let InitialOwnerbal = await ethers.provider.getBalance(owner.address);

    // check enough funds + gas
    expect(InitialOwnerbal).to.be.at.least(TRANSFER_VALUE * 1.1);

    // Deploy the contract
    const singleTransfer = await hre.deployContractWithSigner("SingleTransfer", owner);

    let data = singleTransfer.interface.encodeFunctionData("doTransfer", [randomAccount, TRANSFER_VALUE]);

    let transaction: TransactionRequest = {
      to: singleTransfer.address,
      from: owner.address,
      gasLimit: 250000,
      data: data,
      value: TRANSFER_VALUE
    };

    await expect(ethers.provider.call(transaction)).not.to.be.rejected;
    hre.releaseEthSigner(owner);
  });

  // Disabled in q4-working-branch
  it("should return check gas and funds consistency", async function () {
    let rndAccount = ethers.Wallet.createRandom();

    // 20 ZIL - the txn deployment costs c. 0.33 ZIL.
    const FUND = BigNumber.from(20_000_000_000_000_000_000n);

    const tx = await hre.sendEthTransaction({
      to: rndAccount.address,
      value: FUND
    });

    // Get transaction receipt for the tx
    const receipt = await tx.response.wait();

    let rndAccountProvider = rndAccount.connect(ethers.provider);

    const TRANSFER_VALUE = 100_000_000;

    // We can't use parallizer here since we need a hash of the receipt to inspect gas usage later
    const SingleTransferContract = await ethers.getContractFactory("SingleTransfer", rndAccountProvider);

    const singleTransfer = await SingleTransferContract.deploy({value: TRANSFER_VALUE});

    await singleTransfer.deployed();

    const fee1 = await getFee(singleTransfer.deployTransaction.hash);

    // Need to scale down to ignore miniscule rounding differences from getFee
    const scaleDown = 10000000;

    let newBal = await ethers.provider.getBalance(rndAccountProvider.address);

    const expectedNewBalance = FUND.sub(TRANSFER_VALUE).sub(fee1).div(scaleDown);
    newBal = newBal.div(scaleDown);

    expect(expectedNewBalance).to.be.eq(newBal);
  });
});
