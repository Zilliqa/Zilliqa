const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");
const zilliqa_helper = require("../helper/ZilliqaHelper");

describe("Gas estimation with web3.js 222", function () {
  let contract;
  const tranferFundMinGas = 21000;
  const create2MinGas = 32000;

  describe("When a fund transfer is made", function () {
    it("should return proper estimation [@transactional]", async function () {
      const gasAmount = await web3.eth.estimateGas({
        to: zilliqa_helper.getSecondaryAccountAddress(),
        from: zilliqa_helper.getPrimaryAccountAddress(),
        value: web3.utils.toWei("300", "gwei")
      });
      expect(gasAmount).to.be.at.least(tranferFundMinGas);
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
      expect(gasAmount).to.be.at.least(create2MinGas);
    });
  });
});
