import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";

const FUND = ethers.utils.parseUnits("1", "gwei");

async function getFee(hash: string) {
  const res = await ethers.provider.getTransactionReceipt(hash);
  return res.gasUsed.mul(res.effectiveGasPrice);
}

describe("Events and logs #parallel", function () {
  let contract: Contract;
  before(async function () {
    contract = await hre.deployContract("Event");
  });

  it("Should return 1 log whenever a function with one event is called @block-1", async function () {
    const tx = await contract.one_log();
    const receipt = await ethers.provider.getTransactionReceipt(tx.hash);
    expect(receipt.logs.length).to.be.eq(1);
  });

  it("Should return 2 logs whenever a function with two events is called @block-2", async function () {
    const tx = await contract.two_logs();
    const receipt = await ethers.provider.getTransactionReceipt(tx.hash);
    expect(receipt.logs.length).to.be.eq(2);
  });

  it("Should return log from child contract even if function failed @block-2", async function () {
    const tx = await contract.one_log_and_fail({gasLimit: 250000});
    const receipt = await ethers.provider.getTransactionReceipt(tx.hash);
    console.log("Receipt: " + JSON.stringify(receipt));
    expect(receipt.logs.length).to.be.eq(1);
  });
});
