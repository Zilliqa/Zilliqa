const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");
const helper = require("../helper/GeneralHelper");

// Reference: https://dev.to/yongchanghe/tutorial-using-create2-to-predict-the-contract-address-before-deploying-12cb

describe("Create2 instruction", function () {
  const INITIAL_FUND = 1_000_000;
  let contract;

  before(async function () {
    const Contract = await ethers.getContractFactory("Create2Factory");
    contract = await Contract.deploy();
  });

  describe("Should be able to predict and call create2 contract", function () {

    it("Should predict and deploy create2 contract", async function () {

      const [owner] = await ethers.getSigners();
      const SALT = 1;

      const ownerAddr = owner.address;

      // Use view function to get the bytecode
      const byteCode = await contract.getBytecode(ownerAddr);
      // Ask the contract what the deployed address would be for this salt and owner
      const addrDerived = await contract.getAddress(byteCode, SALT);

      const deployResult = await contract.deploy(SALT, {gasLimit: 25000000});

      // Using the address we calculated, point at the deployed contract
      deployedContract = new web3.eth.Contract(hre.artifacts.readArtifactSync("DeployWithCreate2").abi, addrDerived, {
        from: owner.address
      });

      // Check the owner is correct
      const ownerTest = await deployedContract.methods.getOwner().call();

      expect(ownerTest).to.be.properAddress;
      expect(ownerTest).to.be.eq(ownerAddr);
    });
  });

});
