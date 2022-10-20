const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");

describe("Gas estimation with web3.js", function () {
  let contract;
  const TRANSFER_FUN_MIN_GAS = 21000;
  const CREATE2_MIN_MAS = 32000;

  describe("When a fund transfer is made", function () {
    it("should return proper estimation [@transactional]", async function () {
      const gasAmount = await web3.eth.estimateGas({
        to: web3_helper.getSecondaryAccountAddress(),
        from: web3_helper.getPrimaryAccountAddress(),
        value: web3.utils.toWei("300", "gwei")
      });
      expect(gasAmount).to.be.at.least(TRANSFER_FUN_MIN_GAS);
    });
  });

  describe("When a contract actions is performed", function () {
    before(async function () {
      const gasLimit = "750000";
      const amountPaid = web3.utils.toBN(web3.utils.toWei("300", "gwei"));
      contract = await web3_helper.deploy("ParentContract", {gasLimit, value: amountPaid});
    });

    it("Should return proper gas estimation [@transactional]", async function () {
      const gasAmount = await contract.methods.installChild(123).estimateGas();
      expect(gasAmount).to.be.at.least(CREATE2_MIN_MAS);
    });
  });
});
