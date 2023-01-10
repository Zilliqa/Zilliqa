import {deploy, ScillaContract} from "../../helper/ScillaHelper";
import {expect} from "chai";

describe("Scilla SetGet contract", function () {
  let contract: ScillaContract;
  before(async function () {
    contract = await deploy("SetGet");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should set state correctly", async function () {
    const VALUE = 12;
    await contract.Set(VALUE);
    expect(await contract.value()).to.be.eq(VALUE);
  });

  it("Should contain event data if Get function is called", async function () {
    const tx = await contract.Emit();
    const receipt = tx.getReceipt()!;
    expect(receipt.event_logs?.length).to.be.eq(1);
    expect(receipt.event_logs![0]._eventname).to.be.eq("Emit");
    expect(receipt.event_logs![0].params).to.deep.contain({type: "Uint128", value: "12", vname: "value"});
  });
});
