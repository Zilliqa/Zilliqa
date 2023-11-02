import {expect} from "chai";
import hre, {ethers} from "hardhat";
import {BigNumber, Contract} from "ethers";
import {JsonRpcProvider} from "@ethersproject/providers";
import logDebug from "../../helpers/DebugHelper";
import clc from "cli-color";

// Test basic cases of gas transfer.

class TestCase {
  name: string = "";
  constructContractWith: BigNumber = BigNumber.from(0);
  transferIntoContract: BigNumber = BigNumber.from(0);
  transferOutOfContract: BigNumber = BigNumber.from(0);
  expectedContractInitial: BigNumber = BigNumber.from(0);

  constructor(name: string, constructWith: BigNumber, into: BigNumber, outOf: BigNumber, expectInitial: BigNumber) {
    this.name = name;
    this.constructContractWith = constructWith;
    this.transferIntoContract = into;
    this.transferOutOfContract = outOf;
    this.expectedContractInitial = expectInitial;
  }
}

const expectBalance = (balance1: BigNumber, balance2: BigNumber) => {
  const diff = balance1.sub(balance2).abs();
  console.log(clc.red(diff));
  expect(diff).to.be.at.most("1999999"); // Round error
};

describe("GasTransferTest", function () {
  const ONE_GWEI = BigNumber.from(1_000_000_000);
  const ONE_MWEI = BigNumber.from(1_000_000);
  const testCases = [
    // Values less than 1MWei (10^(18-12)) won't show up because of rounding.
    new TestCase("0", BigNumber.from(ONE_GWEI), BigNumber.from(ONE_GWEI), BigNumber.from(0), BigNumber.from(ONE_GWEI))
    //   new TestCase( ONE_GWEI, BigNumber.from(0), BigNumber.from(0), BigNumber.from(0)),
    //   new TestCase( ONE_GWEI, ONE_GWEI, BigNumber.from(0), BigNumber.from(0)),
  ];
  let provider: JsonRpcProvider;
  var signer;
  let myAddress: string;

  before(async function () {
    provider = ethers.provider;
    signer = provider.getSigner();
    myAddress = await signer.getAddress();
  });

  // @todo use parallelizer to stop this being serial.
  testCases.forEach(function (testCase) {
    let totalGas: BigNumber;
    let contract: Contract;
    let cumulativeGas = BigNumber.from(0);
    let startOfRoundBalance: BigNumber;

    it(`Should run case ${testCase.name}`, async function () {
      let accountBalance = await provider.getBalance(myAddress);
      startOfRoundBalance = accountBalance;

      contract = await hre.deployContract("GasTransferTest", {value: testCase.constructContractWith});

      {
        await contract.deployed();
        const deploymentReceipt = await ethers.provider.getTransactionReceipt(contract.deployTransaction.hash);
        const deploymentTransaction = await ethers.provider.getTransaction(contract.deployTransaction.hash);
        const deploymentGas = deploymentReceipt.gasUsed;
        const gasPrice = deploymentReceipt.effectiveGasPrice;

        logDebug(`Deployed with ${testCase.constructContractWith} and ${JSON.stringify(deploymentReceipt)}`);
        logDebug(`DeploymentGas ${deploymentGas}`);
        logDebug(`Txn was ${JSON.stringify(deploymentTransaction)}`);

        // Our gas pricing shouldn't change (at least, not without updating this test :-) )
        expect(deploymentGas).to.equal(90300);
        totalGas = gasPrice.mul(deploymentGas);
        logDebug(clc.yellow(`TotalGas: ${totalGas}, DeploymentGas: ${deploymentGas}, GasPrice: ${gasPrice}`));
        cumulativeGas = cumulativeGas.add(totalGas);

        logDebug(
          `[0] balance was ${accountBalance} we used ${totalGas} on gas, and gave the contract ${testCase.constructContractWith}`
        );
      }
    });

    it(`My balance should be accountBalance - (testCase.constructContractWith + totalGas)`, async function () {
      let expectedBalance = startOfRoundBalance.sub(testCase.constructContractWith.add(totalGas));
      let currentBalance = await provider.getBalance(myAddress);

      logDebug(
        clc.yellow(
          `TotalGas: ${totalGas}, Expected: ${expectedBalance}, Current: ${currentBalance}, initial: ${startOfRoundBalance}`
        )
      );
      expectBalance(expectedBalance, currentBalance);
      // expect(expectedBalance).to.be.eq(currentBalance);
    });

    it(`Contract balance should be ${testCase.expectedContractInitial}`, async function () {
      const contractBalance = await provider.getBalance(contract.address);
      expect(contractBalance).to.equal(testCase.expectedContractInitial);
    });

    it("Send some money to the contract", async function () {
      const prevContractBalance = await provider.getBalance(contract.address);
      const prevAccountBalance = await provider.getBalance(myAddress);
      const tx = await contract.takeAllMyMoney({value: testCase.transferIntoContract}); // gasLimit: 50000000 } );
      const receipt = await tx.wait();
      logDebug(`Tx: ${JSON.stringify(tx)}`);
      logDebug(`Receipt: ${JSON.stringify(receipt)}`);
      const gasPrice = receipt.effectiveGasPrice;
      const gasUsed = receipt.gasUsed;
      const totalGas = gasPrice.mul(gasUsed);
      cumulativeGas = cumulativeGas.add(totalGas);
      const contractBalance = await provider.getBalance(contract.address);
      const accountBalance = await provider.getBalance(myAddress);
      const transferredIntoContract = contractBalance.sub(prevContractBalance);

      const expectedBalance = prevAccountBalance.sub(totalGas).sub(transferredIntoContract);
      expectBalance(accountBalance, expectedBalance);
      expect(contractBalance).to.equal(testCase.constructContractWith.add(testCase.transferIntoContract));
    });

    it("Now send it back", async function () {
      const prevContractBalance = await provider.getBalance(contract.address);
      const prevAccountBalance = await provider.getBalance(myAddress);
      const tx = await contract.sendBack(testCase.transferOutOfContract);
      const receipt = await tx.wait();
      const gasPrice = receipt.effectiveGasPrice;
      const gasUsed = receipt.gasUsed;
      const totalGas = gasPrice.mul(gasUsed);
      cumulativeGas = cumulativeGas.add(totalGas);
      const contractBalance = await provider.getBalance(contract.address);
      const accountBalance = await provider.getBalance(myAddress);
      const contractDiff = prevContractBalance.sub(contractBalance);
      const expectedBalance = prevAccountBalance.sub(totalGas).add(contractDiff);

      expectBalance(accountBalance, expectedBalance);
      expect(contractDiff).to.equal(testCase.transferOutOfContract);
    });
  });
});
