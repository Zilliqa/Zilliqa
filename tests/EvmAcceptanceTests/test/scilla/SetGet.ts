import {deploy, ScillaContract} from "../../helper/ScillaHelper";
import {expect} from "chai";

describe("Scilla SetGet contract", function () {
  let contract: ScillaContract;
  const VALUE = 12;

  before(async function () {
    contract = await deploy("SetGet");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should set state correctly", async function () {
    await contract.set(VALUE);
    expect(await contract.value()).to.be.eq(VALUE);
  });

  it("Should contain event data if emit transition is called", async function () {
    const tx = await contract.emit();
    expect(tx).to.have.eventLog("Emit");
  });
});
