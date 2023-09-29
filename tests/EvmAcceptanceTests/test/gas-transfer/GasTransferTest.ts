import {expect} from "chai";
import hre, {ethers} from "hardhat";
import {BigNumber} from "ethers";
import {JsonRpcProvider} from "@ethersproject/providers";

// Test basic cases of gas transfer.

class TestCase {
  name: string = "";
  constructContractWith: BigNumber = BigNumber.from(0);
  transferIntoContract: BigNumber = BigNumber.from(0);
  transferOutOfContract: BigNumber = BigNumber.from(0);
  expectedContractInitial: BigNumber = BigNumber.from(0);
  expectedContractAfterTransferIn: BigNumber = BigNumber.from(0);
  expectedContractAfterTransferOut: BigNumber = BigNumber.from(0);

  constructor(
    name: string,
    constructWith: BigNumber,
    into: BigNumber,
    outOf: BigNumber,
    expectInitial: BigNumber,
    expectAfterTransfer: BigNumber,
    expectFinal: BigNumber
  ) {
    this.name = name;
    this.constructContractWith = constructWith;
    this.transferIntoContract = into;
    this.transferOutOfContract = outOf;
    this.expectedContractInitial = expectInitial;
    this.expectedContractAfterTransferIn = expectAfterTransfer;
    this.expectedContractAfterTransferOut = expectFinal;
  }
}

describe("GasTransferTest", function () {
  const ONE_GWEI = BigNumber.from(1_000_000_000);
  const ONE_MWEI = BigNumber.from(1_000_000);
  const DEBUG = false;
  const testCases = [
    // Values less than 1MWei (10^(18-12)) won't show up because of rounding.
    new TestCase(
      "0",
      BigNumber.from(ONE_GWEI),
      BigNumber.from(0),
      BigNumber.from(0),
      BigNumber.from(ONE_GWEI),
      BigNumber.from(ONE_GWEI),
      BigNumber.from(ONE_GWEI)
    )
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
    // Disabled in q4-working-branch
    xit(`Should run case ${testCase.name}`, async function () {
      if (DEBUG) {
        console.log("-----------------------");
      }
      let contractBalance: BigNumber = BigNumber.from(0);
      let accountBalance: BigNumber = await provider.getBalance(myAddress);
      const startOfRoundBalance = accountBalance;
      let cumulativeGas = BigNumber.from(0);
      let block = await provider.getBlock("latest");

      const contract = await hre.deployContract("GasTransferTest", {value: testCase.constructContractWith});

      {
        await contract.deployed();
        const deploymentReceipt = await ethers.provider.getTransactionReceipt(contract.deployTransaction.hash);
        const deploymentTransaction = await ethers.provider.getTransaction(contract.deployTransaction.hash);
        const deploymentGas = deploymentReceipt.gasUsed;
        const gasPrice = deploymentReceipt.effectiveGasPrice;
        if (DEBUG) {
          console.log(`Deployed with ${testCase.constructContractWith} and ${JSON.stringify(deploymentReceipt)}`);
          console.log(`DeploymentGas ${deploymentGas}`);
          console.log(`Txn was ${JSON.stringify(deploymentTransaction)}`);
        }
        // Our gas pricing shouldn't change (at least, not without updating this test :-) )
        expect(deploymentGas).to.equal(90300);
        const totalGas = gasPrice.mul(deploymentGas);
        cumulativeGas = cumulativeGas.add(totalGas);

        if (DEBUG) {
          console.log(
            `[0] balance was ${accountBalance} we used ${totalGas} on gas, and gave the contract ${testCase.constructContractWith}`
          );
        }
        // My balance should be accountBalance - (testCase.constructContractWith + totalGas)
        let expectedBalance = accountBalance.sub(testCase.constructContractWith.add(totalGas));
        let currentBalance = await provider.getBalance(myAddress);
        if (DEBUG) {
          console.log(`[1] expect ${expectedBalance.toBigInt()} got ${currentBalance.toBigInt()}`);
        }
        {
          let iWonGross = currentBalance.sub(expectedBalance);
          let iWonNet = iWonGross.sub(totalGas);
          if (DEBUG) {
            console.log(`[1.5] I won ${iWonGross} Qa - that's my gas plus ${iWonNet}!`);
          }
          // All the money should be accounted for.
          expect(iWonGross).to.equal(0);
        }
        contractBalance = await provider.getBalance(contract.address);
        accountBalance = await provider.getBalance(myAddress);

        if (DEBUG) {
          console.log(
            `[2] Contract has ${contractBalance} of ${testCase.constructContractWith}; account has ${accountBalance}`
          );
          console.log(`[2] C[ontract is ${contract.address}`);
        }
        expect(contractBalance).to.equal(testCase.expectedContractInitial);
      }

      if (DEBUG) {
        console.log(`[2] contract has ${contractBalance}, expected ${testCase.expectedContractInitial}`);
      }

      // Send some money to the contract
      {
        const prevContractBalance = contractBalance;
        const prevAccountBalance = accountBalance;
        const tx = await contract.takeAllMyMoney({value: testCase.transferIntoContract}); // gasLimit: 50000000 } );
        const receipt = await tx.wait();
        if (DEBUG) {
          console.log(`Tx: ${JSON.stringify(tx)}`);
          console.log(`Receipt: ${JSON.stringify(receipt)}`);
        }
        const gasPrice = receipt.effectiveGasPrice;
        const gasUsed = receipt.gasUsed;
        const totalGas = gasPrice.mul(gasUsed);
        cumulativeGas = cumulativeGas.add(totalGas);
        contractBalance = await provider.getBalance(contract.address);
        accountBalance = await provider.getBalance(myAddress);
        if (DEBUG) {
          console.log(
            `[2.5] transfer in ${testCase.transferIntoContract} => contract now has ${contractBalance}, account ${accountBalance}, gas ${totalGas}`
          );
        }
        const transferredIntoContract = contractBalance.sub(prevContractBalance);

        const expectedBalance = prevAccountBalance.sub(totalGas).sub(transferredIntoContract);
        const won = accountBalance.sub(expectedBalance);
        if (DEBUG) {
          console.log(`[2.5] expected balance ${expectedBalance}, actual ${accountBalance}`);
          console.log(
            `[2.5] contract expected balance ${testCase.expectedContractAfterTransferIn}, actual ${contractBalance}`
          );
          console.log(`[2.5] I won ${won}`);
        }
        // All the money should be accounted for.
        expect(won).to.equal(0);
        // And the contract should have the expected balance.
        expect(contractBalance).to.equal(testCase.expectedContractAfterTransferIn);
      }

      // Now send it back
      {
        const prevContractBalance = contractBalance;
        const prevAccountBalance = accountBalance;
        const tx = await contract.sendback(testCase.transferOutOfContract);
        const receipt = await tx.wait();
        const gasPrice = receipt.effectiveGasPrice;
        const gasUsed = receipt.gasUsed;
        const totalGas = gasPrice.mul(gasUsed);
        cumulativeGas = cumulativeGas.add(totalGas);
        contractBalance = await provider.getBalance(contract.address);
        accountBalance = await provider.getBalance(myAddress);
        if (DEBUG) {
          console.log(
            `[2.75] transfer out ${testCase.transferOutOfContract} => contract now has ${contractBalance}, account ${accountBalance}, gas ${totalGas}`
          );
        }
        const contractDiff = prevContractBalance.sub(contractBalance);
        const expectedBalance = prevAccountBalance.sub(totalGas).add(contractDiff);
        const won = accountBalance.sub(expectedBalance);

        if (DEBUG) {
          console.log(`[2.5] expected balance ${expectedBalance}, actual ${accountBalance}`);
          console.log(
            `[2.5] contract expected balance ${testCase.expectedContractAfterTransferOut}, actual ${contractBalance}`
          );
          console.log(`[2.5] I won ${won}`);
        }

        expect(won).to.equal(0);
        expect(contractBalance).to.equal(testCase.expectedContractAfterTransferOut);
      }
      {
        // Now destroy the contract
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
        let expectedBalance = accountBalance.add(contractBalance).sub(destructTotalGas);
        let currentBalance = await provider.getBalance(myAddress);
        const actualDestructGas = myBalanceBeforeDestroy.sub(currentBalance).sub(contractBalanceBeforeDestroy);
        if (DEBUG) {
          console.log(`[3] I expected destruction to cost ${destructTotalGas}; it actually cost ${actualDestructGas}`);
          console.log(`[3] estimated-actual gas = ${estimatedGas.sub(destructGasUsed)}`);
          console.log(`[4] expect ${expectedBalance} got ${currentBalance}`);

          console.log(`[5] Total gas = ${cumulativeGas}`);
        }
        {
          let win = currentBalance.sub(expectedBalance);
          if (DEBUG) {
            console.log(`[5] A net gain on destruction of ${win}`);
          }
        }

        {
          let roundWin = currentBalance.sub(startOfRoundBalance);
          let roundWinMinusGas = roundWin.add(cumulativeGas);
          if (DEBUG) {
            console.log(`[6] A net gain on this round of ${roundWin}`);
            console.log(`[7] win+gas (should be 0) = ${roundWinMinusGas}`);
          }

          // All the money for the whole test should now be accounted for.
          expect(roundWinMinusGas).to.equal(0);
        }
      }
    });
  });
});
