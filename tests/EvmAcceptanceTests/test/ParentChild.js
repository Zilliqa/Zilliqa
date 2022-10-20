const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Parent Child Contract Functionality", function () {
  const INITIAL_FUND = 1_000_000;
  let parentContract;
  before(async function () {
    console.log("Installing parent contract...");
    const Contract = await ethers.getContractFactory("ParentContract");
    parentContract = await Contract.deploy({value: INITIAL_FUND});
  });

  describe("General", function () {
    it(`Should return ${INITIAL_FUND} when getPaidValue is called`, async function () {
      expect(await parentContract.getPaidValue()).to.be.equal(INITIAL_FUND);
    });

    it(`Should return ${INITIAL_FUND} as the balance of the parent contract`, async function () {
      expect(await ethers.provider.getBalance(parentContract.address)).to.be.eq(INITIAL_FUND);
    });
  });

  describe("Install Child", function () {
    let childContract;
    let childContractAddress;
    const CHILD_CONTRACT_VALUE = 12345;
    before(async function () {
      // Because childContractAddress is used in almost all of the following tests, it should be done in `before` block.
      await parentContract.installChild(CHILD_CONTRACT_VALUE, {gasLimit: 25000000});
      childContractAddress = await parentContract.childAddress();
    });

    it("Should instantiate a new child if installChild is called", async function () {
      expect(childContractAddress).to.be.properAddress;
    });

    //it(`Should return ${INITIAL_FUND} as the balance of the child contract`, async function () {
    //  expect(await ethers.provider.getBalance(childContractAddress)).to.be.eq(INITIAL_FUND);
    //});

    //it(`Should return ${CHILD_CONTRACT_VALUE} when read function of the child is called`, async function () {
    //  const [owner] = await ethers.getSigners();
    //  childContract = new web3.eth.Contract(hre.artifacts.readArtifactSync("ChildContract").abi, childContractAddress, {
    //    from: owner.address
    //  });

    //  expect(await childContract.methods.read().call()).to.be.eq(ethers.BigNumber.from(CHILD_CONTRACT_VALUE));
    //});

    //it("Should return parent address if sender function of child is called", async function () {
    //  expect(await childContract.methods.sender().call()).to.be.eq(parentContract.address);
    //});

    //it("Should return all funds from the child to its sender contract if returnToSender is called", async function () {
    //  const [owner] = await ethers.getSigners();
    //  expect(
    //    await childContract.methods.returnToSender().send({gasLimit: 1000000, from: owner.address})
    //  ).to.changeEtherBalances([childContractAddress, parentContract.address], [-INITIAL_FUND, +INITIAL_FUND]);
    //});
  });
});
