import {expect} from "chai";
import hre, {ethers} from "hardhat";
import {BigNumber, Contract} from "ethers";
import {JsonRpcProvider} from "@ethersproject/providers";
import logDebug from "../../helpers/DebugHelper";

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
        cumulativeGas = cumulativeGas.add(totalGas);

        logDebug(
          `[0] balance was ${accountBalance} we used ${totalGas} on gas, and gave the contract ${testCase.constructContractWith}`
        );
      }
    });

    it(`My balance should be accountBalance - (testCase.constructContractWith + totalGas)`, async function () {
      let expectedBalance = startOfRoundBalance.sub(testCase.constructContractWith.add(totalGas));
      let currentBalance = await provider.getBalance(myAddress);

      expect(expectedBalance).to.be.eq(currentBalance);
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
      expect(accountBalance).to.be.eq(expectedBalance);
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

      expect(accountBalance).to.equal(expectedBalance);
      expect(contractDiff).to.equal(testCase.transferOutOfContract);
    });

    it("Now destroy the contract", async () => {
      // The gas measurement here is what GPT suggested. It is horrific, but in the absence of anything else ..
      // It should work for isolated server, but I wouldn't expect it to for anything else
      // - rrw 2023-04-20
      const myBalanceBeforeDestroy = await provider.getBalance(myAddress);
      const contractBalanceBeforeDestroy = await provider.getBalance(contract.address);
      const estimatedGas = await contract.estimateGas.blowUp();
      const tx = await contract.blowUp();
      const receipt = await tx.wait();
      const destructGasPrice = receipt.effectiveGasPrice;
      const destructGasUsed = receipt.gasUsed;
      const destructTotalGas = destructGasPrice.mul(destructGasUsed);
      cumulativeGas = cumulativeGas.add(destructTotalGas);

      // We expect the account to have the previous amount, plus the balance in the contract,
      // minus the amount of gas used to destroy the contract
      let expectedBalance = myBalanceBeforeDestroy.add(contractBalanceBeforeDestroy).sub(destructTotalGas);
      let currentBalance = await provider.getBalance(myAddress);
      const actualDestructGas = myBalanceBeforeDestroy.sub(currentBalance).sub(contractBalanceBeforeDestroy);
      logDebug(`[3] I expected destruction to cost ${destructTotalGas}; it actually cost ${actualDestructGas}`);
      logDebug(`[3] estimated-actual gas = ${estimatedGas.sub(destructGasUsed)}`);
      logDebug(`[4] expect ${expectedBalance} got ${currentBalance}`);

      logDebug(`[5] Total gas = ${cumulativeGas}`);
      {
        let win = currentBalance.sub(expectedBalance);
        logDebug(`[5] A net gain on destruction of ${win}`);
      }

      {
        let roundWin = currentBalance.sub(startOfRoundBalance);
        let roundWinMinusGas = roundWin.add(cumulativeGas);
        logDebug(`[6] A net gain on this round of ${roundWin}`);
        logDebug(`[7] win+gas (should be 0) = ${roundWinMinusGas}`);

        // All the money for the whole test should now be accounted for.
        expect(roundWinMinusGas).to.equal(0);
      }
    });
  });
});
