import {expect} from "chai";
import {parallelizer} from "../helpers";

// TODO: Change the description to something more meaningful.
describe("Blockchain Instructions contract", function () {
  before(async function () {
    console.log("here we gooo000 DEPLOYING...");
    this.contract = await parallelizer.deployContract("BlockchainInstructions");
    console.log("here we gooo000 DEPLOYED...");
  });

  it("Should be deployed successfully", async function () {
    console.log("here we gooo000 ");
    console.log("here we gooo001 ", this.contract.address);
    expect(this.contract.address).to.be.properAddress;
  });

  it("Should return the owner address when getOrigin function is called", async function () {
    console.log("here we gooo000 ALT0");
    const owner = this.contract.signer;
    console.log("here we gooo000 ALT1");
    expect(await this.contract.getOrigin()).to.be.eq(owner.address);
    console.log("here we gooo000 ALT2");
  });
});
