import {expect} from "chai";
import {ethers} from "hardhat";
import {parallelizer} from "../helpers";
import SignerPool from "../helpers/SignerPool";

// FIXME: https://zilliqa-jira.atlassian.net/browse/EM-53
describe.skip("Gas estimation with web3.js", function () {
  const CREATE2_MIN_GAS = 32000;

  describe("When a fund transfer is made", function () {
    it("should return proper estimation [@transactional]", async function () {
      const to = SignerPool.createRandomAccount();
      const signer = await parallelizer.takeSigner();

      const gasAmountEst = await ethers.provider.estimateGas({
        to: to.address,
        from: signer.address,
        value: ethers.utils.parseUnits("300", "gwei")
      });

      const txn = await signer.sendTransaction({
        to: to.address,
        value: ethers.utils.parseUnits("300", "gwei")
      });

      const receipt = await txn.wait();
      expect(gasAmountEst).to.be.equal(receipt.gasUsed);
      parallelizer.releaseSigner(signer);
    });
  });

  describe("When a contract actions is performed", function () {
    // Make sure parent contract is available for child to be called
    before(async function () {
      const amountPaid = ethers.utils.parseUnits("300", "gwei");
      this.contract = await parallelizer.deployContract("ParentContract", {value: amountPaid});
    });

    it("Should return proper gas estimation [@transactional]", async function () {
      const gasAmountEst = await this.contract.estimateGas.installChild(123);
      expect(gasAmountEst).to.be.at.least(CREATE2_MIN_GAS);

      const result = await this.contract.installChild(123);

      expect(result).to.be.not.null;

      const receipt = await result.wait();
      const actualGas = receipt.gasUsed;

      expect(gasAmountEst).to.be.at.least(Math.floor(actualGas * 0.9));
      expect(gasAmountEst).to.be.at.most(Math.floor(actualGas * 1.1));
    });
  });
});
