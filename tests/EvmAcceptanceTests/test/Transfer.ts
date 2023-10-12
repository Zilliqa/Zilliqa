import {assert, expect} from "chai";
import {BigNumber, Contract} from "ethers";
import hre, {ethers} from "hardhat";

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

  // Disabled in q4-working-branch
  xit("should return check gas and funds consistency", async function () {
    let rndAccount = ethers.Wallet.createRandom();

    const FUND = BigNumber.from(200_000_000_000_000_000n);

    const tx = await hre.sendEthTransaction({
      to: rndAccount.address,
      value: FUND
    });

    // Get transaction receipt for the tx
    const receipt = await tx.response.wait();

    rndAccount = rndAccount.connect(ethers.provider);

    const TRANSFER_VALUE = 100_000_000;

    // We can't use parallizer here since we need a hash of the receipt to inspect gas usage later
    const SingleTransferContract = await ethers.getContractFactory("SingleTransfer", rndAccount);

    const singleTransfer = await SingleTransferContract.deploy({value: TRANSFER_VALUE});

    await singleTransfer.deployed();

    const fee1 = await getFee(singleTransfer.deployTransaction.hash);

    // Need to scale down to ignore miniscule rounding differences from getFee
    const scaleDown = 10000000;

    let newBal = await ethers.provider.getBalance(rndAccount.address);

    const expectedNewBalance = FUND.sub(TRANSFER_VALUE).sub(fee1).div(scaleDown);
    newBal = newBal.div(scaleDown);

    expect(expectedNewBalance).to.be.eq(newBal);
  });
});
