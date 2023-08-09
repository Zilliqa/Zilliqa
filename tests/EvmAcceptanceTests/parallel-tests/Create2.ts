import {Block, Scenario, scenario, xit, it} from "../helpers"
import { Contract } from "ethers";
import { expect } from "chai";
import hre, {web3} from "hardhat";

// Reference: https://dev.to/yongchanghe/tutorial-using-create2-to-predict-the-contract-address-before-deploying-12cb

export const create2Scenario = function(contract: Contract): Scenario {
  return scenario("create2",
    it("Should predict and deploy create2 contract", async function () {
      const owner = contract.signer;
      const SALT = 1;

      const ownerAddr = await owner.getAddress();

      // Use view function to get the bytecode
      const byteCode = await contract.getBytecode(ownerAddr);
      // Ask the contract what the deployed address would be for salt and owner
      const addrDerived = await contract.getAddress(byteCode, SALT);

      const deployResult = await contract.deploy(SALT, {gasLimit: 25000000});

      // Using the address we calculated, point at the deployed contract
      const deployedContract = new web3.eth.Contract(
        hre.artifacts.readArtifactSync("DeployWithCreate2").abi,
        addrDerived,
        {
          from: ownerAddr
        }
      );

      // Check the owner is correct
      const ownerTest = await deployedContract.methods.getOwner().call();

      expect(ownerTest).to.be.properAddress;
      expect(ownerTest).to.be.eq(ownerAddr);
    },
    Block.BLOCK_1
    ),
  );
}