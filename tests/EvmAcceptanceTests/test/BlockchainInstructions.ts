import {expect} from "chai";
import hre from "hardhat";
import {Contract} from "ethers";

// TODO: Change the description to something more meaningful.
describe("Blockchain Instructions contract #parallel", function () {
  let contract: Contract;
  before(async function () {
    contract = await hre.deployContract("BlockchainInstructions");
  });

  it("Should be deployed successfully @block-1", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should return the owner address when getOrigin function is called @block-1", async function () {
    const owner = contract.signer;
    expect(await contract.getOrigin()).to.be.eq(await owner.getAddress());
  });
});
