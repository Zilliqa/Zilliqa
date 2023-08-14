import {expect} from "chai";
import hre, {web3} from "hardhat";
import {Contract} from "ethers";

// Reference: https://dev.to/yongchanghe/tutorial-using-create2-to-predict-the-contract-address-before-deploying-12cb

describe("Create2 instruction #parallel", function () {
  let contract: Contract;
  before(async function () {
    contract = await hre.deployContract("Create2Factory");
  });

  it("Should predict and deploy create2 contract @block-1", async function () {
    const owner = contract.signer;
    const SALT = 1;

    const ownerAddr = await owner.getAddress();

    // Use view function to get the bytecode
    const byteCode = await contract.getBytecode(ownerAddr);
    // Ask the contract what the deployed address would be for this salt and owner
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
  });
});
