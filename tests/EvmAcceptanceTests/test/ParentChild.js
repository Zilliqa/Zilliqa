const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Parent Child Contract Functionality", function () {
  const INITIAL_FUND = 1_000;
  let parentContract;
  before(async function () {
    const Contract = await ethers.getContractFactory("ParentContract");
    parentContract = await Contract.deploy({value: INITIAL_FUND});
  });

  describe("General", function () {
    it(`Should return ${INITIAL_FUND} when getPaidValue is called`, async function () {
      expect(await parentContract.getPaidValue()).to.be.equal(INITIAL_FUND);
    });

    // FIXME: In ZIL-4883
    xit(`Should return ${INITIAL_FUND} as the balance of the parent contract`, async function () {
      expect(await ethers.provider.getBalance(parentContract.address)).to.be.eq(INITIAL_FUND);
    });
  });

  describe("Install Child", function () {
    let childContract;
    let childContractAddress;
    const CHILD_CONTRACT_VALUE = 12345;

    // FIXME: In ZIL-4885
    xit("Should instantiate a new child if installChild is called", async function () {
      await parentContract.installChild(CHILD_CONTRACT_VALUE);
      childContractAddress = await parentContract.childAddress();

      expect(childContractAddress).to.be.properAddress;
    });

    // FIXME: In ZIL-4885
    xit(`Should return ${INITIAL_FUND} as the balance of the child contract`, async function () {
      expect(await ethers.provider.getBalance(childContractAddress)).to.be.eq(INITIAL_FUND);
    });

    // FIXME: In ZIL-4885
    xit(`Should return ${CHILD_CONTRACT_VALUE} when read function of the child is called`, async function () {
      const [owner] = await ethers.getSigners();
      childContract = new web3.eth.Contract(hre.artifacts.readArtifactSync("ChildContract").abi, childContractAddress, {
        from: owner.address
      });

      expect(await childContract.methods.read().call()).to.be.eq(ethers.BigNumber.from(CHILD_CONTRACT_VALUE));
    });

    // FIXME: In ZIL-4885
    xit("Should return parent address if sender function of child is called", async function () {
      expect(await childContract.methods.sender().call()).to.be.eq(parentContract.address);
    });

    // FIXME: In ZIL-4885
    xit("Should return all funds from the child to its sender contract if returnToSender is called", async function () {
      const [owner] = await ethers.getSigners();
      await childContract.methods.returnToSender().send({gasLimit: 1000000, from: owner.address});

      expect(await ethers.provider.getBalance(childContractAddress)).to.be.eq(0);
      expect(await ethers.provider.getBalance(parentContract.address)).to.be.eq(INITIAL_FUND);
    });
  });
});
