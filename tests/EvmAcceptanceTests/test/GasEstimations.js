const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");

describe("Gas estimation with web3.js", function () {
  let contract;
  const TRANSFER_FUN_MIN_GAS = 21000;
  const CREATE2_MIN_GAS = 32000;

  describe("When a fund transfer is made", function () {
    it("should return proper estimation [@transactional]", async function () {
      const gasAmountEst = await web3.eth.estimateGas({
        to: web3_helper.getSecondaryAccountAddress(),
        from: web3_helper.getPrimaryAccountAddress(),
        value: web3.utils.toWei("300", "gwei")
      });

      const [payer] = await ethers.getSigners();
      const txSend = await payer.sendTransaction({
          to: web3_helper.getSecondaryAccountAddress(),
          value: web3.utils.toWei("300", "gwei")
        })

      const gasAmount = await web3.eth.getTransaction(txSend.hash);
      expect(gasAmountEst).to.be.equal(gasAmount.gas);
    });
  });

  describe("When a contract actions is performed", function () {
    // Make sure parent contract is available for child to be called
    before(async function () {
      const gasLimit = "750000";
      const amountPaid = web3.utils.toBN(web3.utils.toWei("300", "gwei"));
      contract = await web3_helper.deploy("ParentContract", {gasLimit, value: amountPaid});
    });

    it("Should return proper gas estimation [@transactional]", async function () {
      const gasAmountEst = await contract.methods.installChild(123).estimateGas();
      expect(gasAmountEst).to.be.at.least(CREATE2_MIN_GAS);

      const result = await contract.methods
        .installChild(123)
        .send({gasLimit: 420000000, from: web3_helper.getPrimaryAccountAddress()});

      expect(result).to.be.not.null;
      const actualGas = result.gasUsed;

      expect(gasAmountEst).to.be.at.least(actualGas * 0.9);
      expect(gasAmountEst).to.be.at.most(actualGas * 1.1);
    });
  });
});
