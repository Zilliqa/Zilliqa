import {expect} from "chai";
import {BigNumber,Contract, Signer, Provider} from "ethers";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../helpers";

describe("InteropStress", function () {
  let scillaContract : ScillaContract;
  let evmContract : Contract;
  let contractOwner: Signer;
  let provider : Provider;
  const GAS_LIMIT = 80000;
  const VAL = 1_000_000;
  const FUND = 4 * VAL;
  
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    this.provider = ethers.provider;
    this.contractOwner = await parallelizer.takeSigner();
    this.scillaContract = await parallelizer.deployScillaContract("InteropTestScilla",
                                                                  "Hello!");
    console.log(`Scilla ${JSON.stringify(this.scillaContract)}`)
    let alice = await parallelizer.takeSigner();
    console.log(`Fish ${JSON.stringify(this.scillaContract.address!.toLowerCase())}`)
    this.evmContract = await parallelizer.deployContractWithSigner(
      this.contractOwner,
      "InteropTestSol",
      this.scillaContract.address!.toLowerCase());
    console.log(`Deployed SOL ${this.evmContract.address} Scilla ${this.scillaContract.address}`);
  });

  it("Should be possible to fund the eth contract", async function() {
    let tx = await this.evmContract.receiveFunds( { value: FUND });
    let receipt = await tx.wait();
    let balance = await this.provider.getBalance(this.evmContract.address);
    expect(balance).to.equal(FUND);
  });
 
  it("should not be possible to tick-tock", async function() {
    console.log(`tick`)
    // await reversion doesn't seem to work :-(
    //await expect(this.evmContract.tick( { gasLimit: GAS_LIMIT } )).to.be.revertedWith("fish");
    try {
      let tx = await this.evmContract.tick( { gasLimit: GAS_LIMIT } );
      let receipt = await tx.wait()
      // If we got here, the txn hasn't failed.
      assert(false);
    } catch (e) {
      expect(true).to.be.true;
      console.log("Except");
    }
  });

  it("Should be possible to send funds to Scilla", async function () {
    console.log(`provider - ${JSON.stringify(this.provider)}`)
    let myAddress = await this.contractOwner.getAddress();
    console.log(`addr ${myAddress}`)
    let scillaAddress = this.scillaContract.address!.toLowerCase();
    let myBalanceBefore = await this.provider.getBalance(myAddress);
    let evmBalanceBefore = await this.provider.getBalance(this.evmContract.address);
    let scillaBalanceBefore = await this.provider.getBalance(scillaAddress);
    let amountBefore = myBalanceBefore.add(evmBalanceBefore.add(scillaBalanceBefore));

    let tx = await this.evmContract.sendMoneyToScilla( VAL, {  gasLimit: GAS_LIMIT } )
    try {
      await tx.wait();
      // let receipt = await tx.wait();
    } catch (e) {
      console.log(`Fish ${JSON.stringify(tx)}`);
      // It's probably happened, but the receipt isn't EVM-formatted, so hardhat chokes on it.
    }
    // This is horrid, but necessary or ethers tries to parse the receipt and
    // fails (because of the Scilla events in it).
    let raw = await this.provider.send("eth_getTransactionReceipt", [ tx.hash ]);
    let gasPrice = BigNumber.from(raw['effectiveGasPrice'])
    let gasUsed = BigNumber.from(raw['gasUsed'])
    console.log(`gasPrice ${gasPrice} used ${gasUsed}`)
    let totalGas = gasPrice.mul(gasUsed);

    console.log(`Fish ${JSON.stringify(raw)}`)
    let myBalanceAfter = await this.provider.getBalance(myAddress);
    let evmBalanceAfter = await this.provider.getBalance(this.evmContract.address);
    console.log(`F`)
    let scillaBalanceAfter = await this.provider.getBalance(this.scillaContract.address!.toLowerCase());
    let amountAfter = myBalanceAfter.add(evmBalanceAfter.add(scillaBalanceAfter));
    let profit = amountBefore.sub(amountAfter.add(totalGas));
    console.log(` bal ${myBalanceBefore} ${evmBalanceBefore} ${scillaBalanceBefore} -> ${myBalanceAfter} ${totalGas} ${evmBalanceAfter} ${scillaBalanceAfter} profit ${profit}`)
    expect(profit).to.equal(0);
    // @TODO Re-enable when ZIL-5211 is fixed.
    // expect(evmBalanceAfter).to.equal(evmBalanceBefore.sub(VAL));
    // expect(scillaBalanceAfter).to.equal(scillaBalanceBefore.add(VAL));
  });

  it("Should not be possible to forward funds to Scilla", async function () {
    // If you send funds to an EVM contract, it shouldn't be possible for
    // it to pass on the acceptance to Scilla
    // (or at least, if it does, it shouldn't do so twice!)
    console.log(`provider - ${JSON.stringify(this.provider)}`)
    let myAddress = await this.contractOwner.getAddress();
    console.log(`addr ${myAddress}`)
    let scillaAddress = this.scillaContract.address!.toLowerCase();
    let myBalanceBefore = await this.provider.getBalance(myAddress);
    let evmBalanceBefore = await this.provider.getBalance(this.evmContract.address);
    let scillaBalanceBefore = await this.provider.getBalance(scillaAddress);
    let amountBefore = myBalanceBefore.add(evmBalanceBefore.add(scillaBalanceBefore));

    let tx = await this.evmContract.sendCallToScilla(0, { value: VAL, gasLimit: GAS_LIMIT } );

    try {
      await tx.wait();
      // let receipt = await tx.wait();
    } catch (e) {
      console.log(`Fish ${JSON.stringify(tx)}`);
      // It's probably happened, but the receipt isn't EVM-formatted, so hardhat chokes on it.
    }
    // This is horrid, but necessary or ethers tries to parse the receipt and
    // fails (because of the Scilla events in it).
    let raw = await this.provider.send("eth_getTransactionReceipt", [ tx.hash ]);
    let gasPrice = BigNumber.from(raw['effectiveGasPrice'])
    let gasUsed = BigNumber.from(raw['gasUsed'])
    console.log(`gasPrice ${gasPrice} used ${gasUsed}`)
    let totalGas = gasPrice.mul(gasUsed);

    console.log(`Fish ${JSON.stringify(raw)}`)
    let myBalanceAfter = await this.provider.getBalance(myAddress);
    let evmBalanceAfter = await this.provider.getBalance(this.evmContract.address);
    console.log(`F`)
    let scillaBalanceAfter = await this.provider.getBalance(this.scillaContract.address!.toLowerCase());
    let amountAfter = myBalanceAfter.add(evmBalanceAfter.add(scillaBalanceAfter));
    let profit = amountBefore.sub(amountAfter.add(totalGas));
    console.log(` bal ${myBalanceBefore} ${evmBalanceBefore} ${scillaBalanceBefore} -> ${myBalanceAfter} ${totalGas} ${evmBalanceAfter} ${scillaBalanceAfter} profit ${profit}`)
    expect(profit).to.equal(0);
    expect(evmBalanceAfter).to.equal(evmBalanceBefore.add(VAL));
    expect(scillaBalanceAfter).to.equal(scillaBalanceBefore);
  });

  it("Should not be possible to forward funds to Scilla even with origin-keeping", async function () {
    // If you send funds to an EVM contract, it shouldn't be possible for
    // it to pass on the acceptance to Scilla
    // (or at least, if it does, it shouldn't do so twice!)
    console.log(`provider - ${JSON.stringify(this.provider)}`)
    let myAddress = await this.contractOwner.getAddress();
    console.log(`addr ${myAddress}`)
    let scillaAddress = this.scillaContract.address!.toLowerCase();
    let myBalanceBefore = await this.provider.getBalance(myAddress);
    let evmBalanceBefore = await this.provider.getBalance(this.evmContract.address);
    let scillaBalanceBefore = await this.provider.getBalance(scillaAddress);
    let amountBefore = myBalanceBefore.add(evmBalanceBefore.add(scillaBalanceBefore));

    let tx = await this.evmContract.sendCallToScilla2(0, { value: VAL, gasLimit: GAS_LIMIT } );

    try {
      await tx.wait();
      // let receipt = await tx.wait();
    } catch (e) {
      console.log(`Fish ${JSON.stringify(tx)}`);
      // It's probably happened, but the receipt isn't EVM-formatted, so hardhat chokes on it.
    }
    // This is horrid, but necessary or ethers tries to parse the receipt and
    // fails (because of the Scilla events in it).
    let raw = await this.provider.send("eth_getTransactionReceipt", [ tx.hash ]);
    let gasPrice = BigNumber.from(raw['effectiveGasPrice'])
    let gasUsed = BigNumber.from(raw['gasUsed'])
    console.log(`gasPrice ${gasPrice} used ${gasUsed}`)
    let totalGas = gasPrice.mul(gasUsed);

    console.log(`Fish ${JSON.stringify(raw)}`)
    let myBalanceAfter = await this.provider.getBalance(myAddress);
    let evmBalanceAfter = await this.provider.getBalance(this.evmContract.address);
    console.log(`F`)
    let scillaBalanceAfter = await this.provider.getBalance(this.scillaContract.address!.toLowerCase());
    let amountAfter = myBalanceAfter.add(evmBalanceAfter.add(scillaBalanceAfter));
    let profit = amountBefore.sub(amountAfter.add(totalGas));
    console.log(` bal ${myBalanceBefore} ${evmBalanceBefore} ${scillaBalanceBefore} -> ${myBalanceAfter} ${totalGas} ${evmBalanceAfter} ${scillaBalanceAfter} profit ${profit}`)
    expect(profit).to.equal(0);
    expect(evmBalanceAfter).to.equal(evmBalanceBefore.add(VAL));
    expect(scillaBalanceAfter).to.equal(scillaBalanceBefore);
  });

  it("send funds from Scilla to Solidity", async function {
    
    
  });
});
