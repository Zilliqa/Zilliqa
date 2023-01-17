import {expect} from "chai";
import {parallelizer} from "../helpers";
import hre, {web3} from "hardhat";

// Reference: https://dev.to/yongchanghe/tutorial-using-create2-to-predict-the-contract-address-before-deploying-12cb

describe("Create2 instruction", function () {
  before(async function () {
    this.contract = await parallelizer.deployContract("Create2Factory");
  });

  describe("Should be able to predict and call create2 contract", function () {
    it("Should predict and deploy create2 contract", async function () {
      const owner = this.contract.signer;
      const SALT = 1;

      const ownerAddr = owner.address;

      // Use view function to get the bytecode
      const byteCode = await this.contract.getBytecode(ownerAddr);
      // Ask the contract what the deployed address would be for this salt and owner
      const addrDerived = await this.contract.getAddress(byteCode, SALT);

      const deployResult = await this.contract.deploy(SALT, {gasLimit: 25000000});

      // Using the address we calculated, point at the deployed contract
      const deployedContract = new web3.eth.Contract(
        hre.artifacts.readArtifactSync("DeployWithCreate2").abi,
        addrDerived,
        {
          from: owner.address
        }
      );

      // Check the owner is correct
      const ownerTest = await deployedContract.methods.getOwner().call();

      expect(ownerTest).to.be.properAddress;
      expect(ownerTest).to.be.eq(ownerAddr);
    });
  });
});
