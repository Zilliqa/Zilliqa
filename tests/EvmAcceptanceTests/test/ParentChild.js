const {expect} = require("chai");
const {ethers, web3} = require("hardhat");
const hre = require("hardhat");
const helper = require("../helper/GeneralHelper");

describe("Parent Child Contract Functionality", function () {
  const INITIAL_FUND = 1_000_000;
  before(async function () {
    const Contract = await ethers.getContractFactory("ParentContract");
    this.parentContract = await Contract.deploy({value: INITIAL_FUND});
  });

  describe("General", function () {
    it(`Should return ${INITIAL_FUND} when getPaidValue is called`, async function () {
      expect(await parentContract.getPaidValue(), `Contract Address: ${this.parentContract.address}`).to.be.equal(
        INITIAL_FUND
      );
    });

    it(`Should return ${INITIAL_FUND} as the balance of the parent contract`, async function () {
      expect(
        await ethers.provider.getBalance(this.parentContract.address),
        `Contract Address: ${this.parentContract.address}`
      ).to.be.eq(INITIAL_FUND);
    });
  });

  describe("Install Child", function () {
    const CHILD_CONTRACT_VALUE = 12345;
    before(async function () {
      // Because childContractAddress is used in almost all of the following tests, it should be done in `before` block.
      this.installedChild = await this.parentContract.installChild(CHILD_CONTRACT_VALUE);
      this.childContractAddress = await this.parentContract.childAddress();
    });

    it("Should instantiate a new child if installChild is called", async function () {
      expect(childContractAddress).to.be.properAddress;
    });

    it(`Should return ${INITIAL_FUND} as the balance of the child contract`, async function () {
      expect(
        await ethers.provider.getBalance(childContractAddress),
        `Contract Address: ${this.childContractAddress}`
      ).to.be.eq(INITIAL_FUND);
    });

    it(`Should return ${CHILD_CONTRACT_VALUE} when read function of the child is called`, async function () {
      const [owner] = await ethers.getSigners();
      childContract = new web3.eth.Contract(hre.artifacts.readArtifactSync("ChildContract").abi, childContractAddress, {
        from: owner.address
      });
      expect(await childContract.methods.read().call()).to.be.eq(ethers.BigNumber.from(CHILD_CONTRACT_VALUE));
    });

    xit("Should create a transaction trace after child creation", async function () {
      const METHOD = "debug_traceTransaction";

      await helper.callEthMethod(METHOD, 1, [installedChild.hash], (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.isString(result.result, "Expected to be populated");
      });
    });

    it("Should return parent address if sender function of child is called", async function () {
      expect(await childContract.methods.sender().call()).to.be.eq(this.parentContract.address);
    });

    it("Should return all funds from the child to its sender contract if returnToSender is called", async function () {
      const [owner] = await ethers.getSigners();
      expect(
        await childContract.methods.returnToSender().send({gasLimit: 1000000, from: owner.address})
      ).to.changeEtherBalances([childContractAddress, this.parentContract.address], [-INITIAL_FUND, +INITIAL_FUND]);
    });
  });
});
