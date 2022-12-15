const {expect} = require("chai");
const {ethers} = require("hardhat");
const hre = require("hardhat");
const helper = require("../helper/GeneralHelper");
const parallelizer = require("../helper/Parallelizer");

describe("Parent Child Contract Functionality", function () {
  const INITIAL_FUND = 1_000_000;
  before(async function () {
    this.parentContract = await parallelizer.deployContract("ParentContract", {value: INITIAL_FUND});
  });

  describe("General", function () {
    it(`Should return ${INITIAL_FUND} when getPaidValue is called`, async function () {
      expect(await this.parentContract.getPaidValue()).to.be.equal(INITIAL_FUND);
    });

    it(`Should return ${INITIAL_FUND} as the balance of the parent contract`, async function () {
      expect(await ethers.provider.getBalance(this.parentContract.address)).to.be.eq(INITIAL_FUND);
    });
  });

  describe("Install Child", function () {
    const CHILD_CONTRACT_VALUE = 12345;
    before(async function () {
      // Because childContractAddress is used in almost all of the following tests, it should be done in `before` block.
      this.installedChild = await this.parentContract.installChild(CHILD_CONTRACT_VALUE, {gasLimit: 25000000});
      this.childContractAddress = await this.parentContract.childAddress();
    });

    it("Should instantiate a new child if installChild is called", async function () {
      expect(this.childContractAddress).to.be.properAddress;
    });

    it(`Should return ${INITIAL_FUND} as the balance of the child contract`, async function () {
      expect(await ethers.provider.getBalance(this.childContractAddress)).to.be.eq(INITIAL_FUND);
    });

    it(`Should return ${CHILD_CONTRACT_VALUE} when read function of the child is called`, async function () {
      const Contract = ethers.getContractFactory("ChildContract");
      this.childContract = await hre.ethers.getContractAt("ChildContract", this.childContractAddress);
      expect(await this.childContract.read()).to.be.eq(CHILD_CONTRACT_VALUE);
    });

    xit("Should create a transaction trace after child creation", async function () {
      const METHOD = "debug_traceTransaction";

      await helper.callEthMethod(METHOD, 1, [this.installedChild.hash], (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.isString(result.result, "Expected to be populated");
      });
    });

    it("Should return parent address if sender function of child is called", async function () {
      expect(await this.childContract.sender()).to.be.eq(this.parentContract.address);
    });

    it("Should return all funds from the child to its sender contract if returnToSender is called", async function () {
      const owner = this.parentContract.signer;
      expect(await this.childContract.returnToSender()).to.changeEtherBalances(
        [this.childContractAddress, this.parentContract.address],
        [-INITIAL_FUND, +INITIAL_FUND]
      );
    });
  });
});
